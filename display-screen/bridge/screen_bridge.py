#!/usr/bin/env python3
"""
screen_bridge.py — one port that serves the DarcOS "screen".

On a single port (default 9090) this:
  * serves the built React frontend  (cs452-nix-frontend/dist)  over HTTP
  * /screen-ws  — byte-transparent relay to the OS console (QEMU)
  * /state-ws   — JSON broadcast of train state extracted from OSC 452 frames
                  embedded in the console byte stream by tc1_entry()

OSC 452 format emitted by the kernel:
  ESC ] 452 ; <JSON> BEL
xterm.js ignores unknown OSC sequences; screen_bridge.py intercepts them here
and forwards the JSON payload to every connected /state-ws client.
"""
import asyncio
import base64
import hashlib
import os
import signal
import socket
import struct

WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
ROOT = os.path.dirname(os.path.abspath(__file__))               # .../display-screen/bridge
REPO = os.path.abspath(os.path.join(ROOT, "..", ".."))          # repo root
DIST = os.path.join(os.path.dirname(ROOT), "cs452-nix-frontend", "dist")
IMG = os.environ.get("KERNEL_IMG_NAME", "os/0-d273liu.img")
LISTEN_PORT = int(os.environ.get("BRIDGE_PORT", "9090"))

MIME = {
    ".html": "text/html; charset=utf-8", ".js": "text/javascript",
    ".mjs": "text/javascript", ".css": "text/css", ".json": "application/json",
    ".svg": "image/svg+xml", ".png": "image/png", ".ico": "image/x-icon",
    ".woff": "font/woff", ".woff2": "font/woff2", ".map": "application/json",
}

# All connected /state-ws writers; state frames are broadcast to all of them.
_state_clients: set = set()


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


async def read_request(reader):
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = await reader.read(1024)
        if not chunk:
            return None, None
        data += chunk
        if len(data) > 65536:
            break
    head = data.split(b"\r\n\r\n", 1)[0].split(b"\r\n")
    first = head[0].decode("latin1", "replace").split(" ")
    path = first[1] if len(first) >= 2 else "/"
    headers = {}
    for line in head[1:]:
        if b":" in line:
            k, v = line.split(b":", 1)
            headers[k.strip().lower()] = v.strip()
    return path, headers


def serve_static(writer, path):
    path = path.split("?")[0]
    if path in ("", "/"):
        path = "/index.html"
    fp = os.path.normpath(os.path.join(DIST, path.lstrip("/")))
    if not (fp.startswith(DIST) and os.path.isfile(fp)):
        fp = os.path.join(DIST, "index.html")          # SPA fallback
    if not os.path.isfile(fp):
        body = b"screen frontend not built: run `npm run build` in cs452-nix-frontend"
        writer.write(b"HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n"
                     b"Content-Length: " + str(len(body)).encode() + b"\r\n\r\n" + body)
        return
    with open(fp, "rb") as f:
        data = f.read()
    ctype = MIME.get(os.path.splitext(fp)[1], "application/octet-stream")
    writer.write(
        ("HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\n"
         "Cache-Control: no-cache\r\n\r\n" % (ctype, len(data))).encode() + data)


def ws_frame(payload, opcode=2):
    out = bytearray([0x80 | opcode])
    n = len(payload)
    if n < 126:
        out.append(n)
    elif n < 65536:
        out.append(126); out += struct.pack(">H", n)
    else:
        out.append(127); out += struct.pack(">Q", n)
    out += payload
    return bytes(out)


async def ws_read_frame(reader):
    b0, b1 = await reader.readexactly(2)
    opcode = b0 & 0x0F
    masked = b1 & 0x80
    ln = b1 & 0x7F
    if ln == 126:
        ln = struct.unpack(">H", await reader.readexactly(2))[0]
    elif ln == 127:
        ln = struct.unpack(">Q", await reader.readexactly(8))[0]
    mask = await reader.readexactly(4) if masked else b"\x00\x00\x00\x00"
    data = bytearray(await reader.readexactly(ln))
    if masked:
        for i in range(ln):
            data[i] ^= mask[i % 4]
    return opcode, bytes(data)


def kill_group(proc):
    try:
        os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
    except Exception:
        pass


# ---------------------------------------------------------------------------
# OSC 452 parser — stateful, fed byte-by-byte from the OS→WS stream
# ---------------------------------------------------------------------------

class Osc452Parser:
    """
    Intercept OSC 452 sequences in the console byte stream and broadcast the
    JSON payload to all connected /state-ws clients.

    State machine:
      IDLE -> ESC_SEEN -> OSC_SEEN -> NUM_SEEN -> PAYLOAD -> DONE/IDLE
    """
    ST_IDLE    = 0
    ST_ESC     = 1   # saw 0x1b
    ST_OSC     = 2   # saw 0x1b 0x5d (ESC ])
    ST_DIGITS  = 3   # accumulating OSC number digits
    ST_PAY     = 4   # accumulating payload (after ';' separator)

    def __init__(self):
        self._state = self.ST_IDLE
        self._digits = []
        self._payload = []

    def feed(self, byte: int):
        s = self._state
        if s == self.ST_IDLE:
            if byte == 0x1b:
                self._state = self.ST_ESC
        elif s == self.ST_ESC:
            if byte == 0x5d:   # ']'
                self._state = self.ST_OSC
                self._digits = []
            else:
                self._state = self.ST_IDLE
        elif s == self.ST_OSC:
            if 0x30 <= byte <= 0x39:   # digit
                self._digits.append(byte)
                self._state = self.ST_DIGITS
            else:
                self._state = self.ST_IDLE
        elif s == self.ST_DIGITS:
            if 0x30 <= byte <= 0x39:
                self._digits.append(byte)
            elif byte == 0x3b:   # ';'
                num = int(bytes(self._digits).decode())
                if num == 452:
                    self._payload = []
                    self._state = self.ST_PAY
                else:
                    self._state = self.ST_IDLE
            else:
                self._state = self.ST_IDLE
        elif s == self.ST_PAY:
            if byte == 0x07:   # BEL — end of OSC
                json_bytes = bytes(self._payload)
                asyncio.ensure_future(_broadcast_state(json_bytes))
                self._state = self.ST_IDLE
            elif byte == 0x1b:   # ST start (ESC \) — alternative terminator
                self._payload_pending_st = True
                self._state = self.ST_IDLE   # handle below if next byte is \
            else:
                self._payload.append(byte)


async def _broadcast_state(json_bytes: bytes):
    if not _state_clients:
        return
    text = json_bytes.decode("utf-8", "replace")
    frame = ws_frame(json_bytes, 0x1)   # text frame
    dead = set()
    for w in list(_state_clients):
        try:
            w.write(frame)
            await w.drain()
        except Exception:
            dead.add(w)
    _state_clients.difference_update(dead)
    print(f"[state] broadcast -> {len(_state_clients)} clients: {text[:80]}", flush=True)


async def serve_console(reader, writer, key):
    accept = base64.b64encode(hashlib.sha1(key + WS_GUID.encode()).digest()).decode()
    writer.write((
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\nConnection: Upgrade\r\n"
        f"Sec-WebSocket-Accept: {accept}\r\n\r\n"
    ).encode())
    await writer.drain()

    port      = free_port()
    link_port = free_port()
    env = dict(os.environ, CONSOLE="tcp", CONSOLE_PORT=str(port),
               MARKLIN="vhw", LINK_PORT=str(link_port))
    env.setdefault("DISK_ROOT", "/disk")
    proc = await asyncio.create_subprocess_exec(
        "bash", "qemu-rpi4.sh", IMG, cwd=REPO, env=env,
        stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL,
        preexec_fn=os.setsid,
    )
    vhw = await asyncio.create_subprocess_exec(
        "python3", os.path.join(REPO, "tools", "vhw.py"), "--port", str(link_port),
        env=env,
        stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL,
        preexec_fn=os.setsid,
    )
    print(f"[bridge] WS client -> spawned OS (console :{port}, pid {proc.pid}) "
          f"+ vhw (link :{link_port}, pid {vhw.pid})", flush=True)

    qr = qw = None
    for _ in range(120):
        try:
            qr, qw = await asyncio.open_connection("127.0.0.1", port)
            break
        except OSError:
            await asyncio.sleep(0.1)
    if qr is None:
        kill_group(proc)
        writer.close()
        return

    parser = Osc452Parser()

    async def os_to_ws():
        try:
            while True:
                d = await qr.read(4096)
                if not d:
                    break
                for b in d:
                    parser.feed(b)
                writer.write(ws_frame(d, 2))
                await writer.drain()
        except Exception:
            pass

    async def ws_to_os():
        try:
            while True:
                op, payload = await ws_read_frame(reader)
                if op == 0x8:
                    break
                if op in (0x1, 0x2):
                    qw.write(payload)
                    await qw.drain()
                elif op == 0x9:
                    writer.write(ws_frame(payload, 0xA))
                    await writer.drain()
        except Exception:
            pass

    try:
        await asyncio.gather(os_to_ws(), ws_to_os())
    finally:
        kill_group(proc)
        kill_group(vhw)
        try:
            qw.close()
        except Exception:
            pass
        try:
            writer.close()
        except Exception:
            pass
        print(f"[bridge] WS client gone -> OS killed (pid {proc.pid}), vhw killed (pid {vhw.pid})", flush=True)


async def serve_state(reader, writer, key):
    accept = base64.b64encode(hashlib.sha1(key + WS_GUID.encode()).digest()).decode()
    writer.write((
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\nConnection: Upgrade\r\n"
        f"Sec-WebSocket-Accept: {accept}\r\n\r\n"
    ).encode())
    await writer.drain()
    _state_clients.add(writer)
    print(f"[state] client connected ({len(_state_clients)} total)", flush=True)

    # Keep alive: read frames until close
    try:
        while True:
            op, _ = await ws_read_frame(reader)
            if op == 0x8:
                break
    except Exception:
        pass
    finally:
        _state_clients.discard(writer)
        try:
            writer.close()
        except Exception:
            pass
        print(f"[state] client gone ({len(_state_clients)} remaining)", flush=True)


async def handle(reader, writer):
    try:
        path, headers = await read_request(reader)
        if path is None:
            writer.close()
            return
        is_ws = (headers.get(b"upgrade", b"").lower() == b"websocket"
                 and headers.get(b"sec-websocket-key"))
        if is_ws and path.split("?")[0] == "/screen-ws":
            await serve_console(reader, writer, headers[b"sec-websocket-key"])
        elif is_ws and path.split("?")[0] == "/state-ws":
            await serve_state(reader, writer, headers[b"sec-websocket-key"])
        else:
            serve_static(writer, path)
            await writer.drain()
            writer.close()
    except Exception:
        try:
            writer.close()
        except Exception:
            pass


async def main():
    srv = await asyncio.start_server(handle, "0.0.0.0", LISTEN_PORT)
    print(f"[bridge] DarcOS screen on http://0.0.0.0:{LISTEN_PORT}/  (dist={DIST}, img={IMG})", flush=True)
    async with srv:
        await srv.serve_forever()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
