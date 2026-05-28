import { randomUUID } from "node:crypto";
import { distRoot, httpsEnabled, port } from "./config";
import { lanUrls } from "./network";
import { handleApiRequest } from "./routes";
import { serveStatic } from "./static";
import { loadTlsOptions } from "./tls";
import type { ClientData } from "./types";
import { websocketHandlers } from "./websocket";

Bun.serve<ClientData>({
  port,
  hostname: "0.0.0.0",
  tls: httpsEnabled ? loadTlsOptions() : undefined,
  async fetch(req, server) {
    const url = new URL(req.url);

    if (url.pathname === "/ws") {
      const upgraded = server.upgrade(req, {
        data: {
          id: randomUUID(),
          role: null
        }
      });
      return upgraded ? undefined : new Response("Upgrade failed", { status: 400 });
    }

    const apiResponse = await handleApiRequest(req, url);
    if (apiResponse) {
      return apiResponse;
    }

    return serveStatic(distRoot, url.pathname);
  },
  websocket: websocketHandlers
});

const protocol = httpsEnabled ? "https" : "http";
console.log(`Peek ${protocol.toUpperCase()} server listening on ${port}`);
for (const url of lanUrls(port, protocol)) {
  console.log(url);
}
