# DarcOS display-screen

A React/Vite app that renders the **live d273liu-nix console** as a screen in the
browser, using xterm.js over a WebSocket bridge to the kernel's serial console.

```
browser (xterm.js, /screen page)
   │  WebSocket  ws://localhost:8080
   ▼
display-screen/bridge/screen_bridge.py   (in the dev container)
   │  spawns one OS per connection: CONSOLE=tcp + LINK=vhw
   ▼
QEMU raspi4b  serial0 (console)  ◄─ the screen ─►  serial1 (Marklin link → vhw.py)
```

Each browser connection gets its **own** OS instance (one QEMU, two UARTs). The
bridge relays raw bytes, so xterm.js's automatic `ESC[6n` size reply reaches the
kernel — the OS's universal screen size matches the browser terminal, and
keystrokes (including `link …`, games, `htop`) work live.

## Run

### Just run it (one port)

The bridge serves the built frontend **and** the console on a single port (9090):

```bash
cd display-screen/cs452-nix-frontend && npm install && npm run build   # once (-> dist/)
docker compose up -d screen                                            # from the repo root
```

Open **http://localhost:9090/** and click **Screen**. That's it — the page and the
live OS console (`/screen-ws`) both come from `:9090`. Rebuild `dist/` after editing
the frontend; the bridge serves it live (no restart). Stop with `docker compose stop screen`.

### Dev mode (hot reload)

```bash
cd display-screen/cs452-nix-frontend && npm run dev
```

Vite serves on its own port and proxies `/screen-ws` to the bridge on `:9090`
(see `vite.config.js`), so the frontend always connects **same-origin** — no
cross-origin/CSP fuss. Override the bridge port with `BRIDGE_PORT=… docker compose up -d screen`
(and update the `/screen-ws` proxy target / `VITE_BRIDGE_URL` to match).

## Notes / TODO

- Tiers: **cs452-nix-frontend** (the screen UI, fed the CONSOLE/UART1 over the
  thin `bridge`), the **OS** (CS452-faithful), and **cs452-nix-backend** (the
  "overpowered Marklin" — connects to the MARKLIN/UART2 link for system access &
  control). The console→frontend path is the thin one (this `bridge`); the
  backend owns the link UART.
- `cs452-nix-backend/` is currently a **scaffold only** — its Dockerfile
  references Django files that aren't present, so it won't build as-is. The
  screen does **not** need it; today the link UART is served by `tools/vhw.py`
  (the basic Marklin sim), which the backend is meant to supersede.
- `node_modules/`, `.DS_Store`, and `desktop.ini` are committed — add them to
  `.gitignore` and untrack them.
- If the terminal stays blank, check the browser console for a blocked WebSocket
  (CSP). The bridge accepts any origin; if your CSP blocks `ws:`, allow
  `connect-src ws: wss:`.
