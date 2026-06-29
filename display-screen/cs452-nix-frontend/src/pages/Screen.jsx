import { useEffect, useRef } from "react";
import { Terminal } from "@xterm/xterm";
import "@xterm/xterm/css/xterm.css";

/* Must match src/layer1-processes/config.h — default 176×50 */
const UI_DESIGN_SCREEN_COLS = 176;
const UI_DESIGN_SCREEN_ROWS = 50;

const BRIDGE_URL =
  import.meta.env.VITE_BRIDGE_URL ||
  `${location.protocol === "https:" ? "wss" : "ws"}://${location.host}/screen-ws`;

export default function Screen({ topBarHeight = 0 }) {
  const ref = useRef(null);

  useEffect(() => {
    document.documentElement.style.height = "100%";
    document.body.style.margin = "0";
    document.body.style.height = "100%";
    document.body.style.background = "#000";

    const term = new Terminal({
      cursorBlink: false,
      fontFamily: "Menlo, Monaco, Consolas, 'DejaVu Sans Mono', monospace",
      fontSize: 15,
      theme: { background: "#000000", foreground: "#e6e6e6" },
      scrollback: 1000,
      cols: UI_DESIGN_SCREEN_COLS,
      rows: UI_DESIGN_SCREEN_ROWS,
    });
    term.open(ref.current);
    term.resize(UI_DESIGN_SCREEN_COLS, UI_DESIGN_SCREEN_ROWS);
    term.write("\x1b[?25l");
    term.focus();

    const fitDesign = () => {
      term.resize(UI_DESIGN_SCREEN_COLS, UI_DESIGN_SCREEN_ROWS);
      const el = ref.current;
      if (!el) return;
      requestAnimationFrame(() => {
        const canvas = el.querySelector(".xterm-screen");
        if (!canvas) return;
        const cw = canvas.offsetWidth;
        const ch = canvas.offsetHeight;
        el.style.width = `${cw}px`;
        el.style.height = `${ch}px`;
        const wrap = el.parentElement;
        if (!wrap) return;
        const scale = Math.min(
          (wrap.clientWidth - 8) / cw,
          (wrap.clientHeight - 8) / ch,
          4
        );
        el.style.transform = scale > 1 ? `scale(${scale})` : "none";
        el.style.transformOrigin = "center center";
      });
    };
    fitDesign();

    const centerMsg = (text) => {
      const row = Math.max(1, Math.floor((term.rows + 1) / 2));
      const col = Math.max(1, Math.floor((term.cols - (text.length + 2)) / 2) + 1);
      term.write(`\x1b[2J\x1b[${row};${col}H\x1b[1;37;41m ${text} \x1b[0m`);
    };

    const ws = new WebSocket(BRIDGE_URL);
    ws.binaryType = "arraybuffer";
    ws.onopen = () => { fitDesign(); term.focus(); };
    ws.onmessage = (e) => {
      if (typeof e.data === "string") term.write(e.data);
      else term.write(new Uint8Array(e.data));
    };
    ws.onclose = () => centerMsg("disconnected — reload to reconnect");
    ws.onerror = () => {};

    const sub = term.onData((d) => { if (ws.readyState === WebSocket.OPEN) ws.send(d); });
    const onResize = () => fitDesign();
    const onFocus = () => term.focus();
    window.addEventListener("resize", onResize);
    window.addEventListener("focus", onFocus);
    ref.current?.addEventListener("click", onFocus);

    return () => {
      window.removeEventListener("resize", onResize);
      window.removeEventListener("focus", onFocus);
      sub.dispose();
      ws.close();
      term.dispose();
    };
  }, []);

  const availH = `calc(100vh - ${topBarHeight}px)`;

  return (
    <div
      style={{
        position: "fixed",
        top: topBarHeight,
        left: 0,
        right: 0,
        bottom: 0,
        display: "flex",
        alignItems: "center",
        justifyContent: "center",
        background: "#000",
      }}
    >
      <div
        ref={ref}
        style={{
          aspectRatio: `${UI_DESIGN_SCREEN_COLS} / ${UI_DESIGN_SCREEN_ROWS}`,
          width: `min(100vw, calc(${availH} * ${UI_DESIGN_SCREEN_COLS} / ${UI_DESIGN_SCREEN_ROWS}))`,
          height: `min(${availH}, calc(100vw * ${UI_DESIGN_SCREEN_ROWS} / ${UI_DESIGN_SCREEN_COLS}))`,
          maxWidth: "100vw",
          maxHeight: availH,
          padding: "4px",
          boxSizing: "border-box",
        }}
      />
    </div>
  );
}
