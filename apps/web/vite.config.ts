import { existsSync, readFileSync } from "node:fs";
import { dirname, join, normalize } from "node:path";
import { fileURLToPath } from "node:url";
import { defineConfig } from "vite";

const root = dirname(fileURLToPath(import.meta.url));
const backendPort = Number(process.env.PORT ?? 3001);
const httpsEnabled = process.env.PEEK_HTTPS === "1";
const tlsCertPath = normalize(process.env.PEEK_TLS_CERT ?? join(root, ".peek-data", "certs", "peek-local.crt"));
const tlsKeyPath = normalize(process.env.PEEK_TLS_KEY ?? join(root, ".peek-data", "certs", "peek-local.key"));
const viteHttps =
  httpsEnabled && existsSync(tlsCertPath) && existsSync(tlsKeyPath)
    ? {
        cert: readFileSync(tlsCertPath),
        key: readFileSync(tlsKeyPath)
      }
    : undefined;
const backendHttpProtocol = httpsEnabled ? "https" : "http";
const backendWsProtocol = httpsEnabled ? "wss" : "ws";

export default defineConfig({
  server: {
    host: "0.0.0.0",
    port: 5173,
    https: viteHttps,
    proxy: {
      "/api": {
        target: `${backendHttpProtocol}://localhost:${backendPort}`,
        secure: false
      },
      "/ws": {
        target: `${backendWsProtocol}://localhost:${backendPort}`,
        secure: false,
        ws: true
      }
    }
  }
});
