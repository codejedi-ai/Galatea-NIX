import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// https://vitejs.dev/config/
export default defineConfig(() => ({
  base: "/",
  plugins: [react()],
  server: {
    proxy: {
      "/api": {
        target: "http://127.0.0.1:8000",
        changeOrigin: true,
      },
      "/admin": {
        target: "http://127.0.0.1:8000",
        changeOrigin: true,
      },
      // CONSOLE / UART1 (the screen): Vite proxies this WebSocket to the
      // console bridge (docker compose up screen, :9090) so the frontend
      // connects same-origin.
      "/screen-ws": {
        target: "ws://127.0.0.1:9090",
        ws: true,
        changeOrigin: true,
      },
    },
    headers: {
      "Content-Security-Policy": "default-src * 'unsafe-inline' 'unsafe-eval' data: blob:;"
    }
  }
}));
