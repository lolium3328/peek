import { existsSync } from "node:fs";
import { extname, join, normalize } from "node:path";
import { fileURLToPath } from "node:url";
import { dirname } from "node:path";
import { handleRenderRequest } from "./render";

const serverDir = dirname(fileURLToPath(import.meta.url));
const webRoot = normalize(join(serverDir, ".."));
const repoRoot = normalize(join(webRoot, "..", ".."));
const distRoot = join(webRoot, "dist");
const previewRoot = join(repoRoot, ".peek-preview");
const port = Number(process.env.PEEK_SIM_PORT ?? 3201);

Bun.serve({
  port,
  hostname: "0.0.0.0",
  async fetch(req) {
    const url = new URL(req.url);

    if (url.pathname === "/api/health") {
      return Response.json({ data: { ok: true, serverTime: Date.now() } });
    }

    if (url.pathname === "/api/render" && req.method === "POST") {
      return await handleRenderRequest(req);
    }

    if (url.pathname.startsWith("/frames/")) {
      return servePreviewFrame(url.pathname);
    }

    return serveStatic(url.pathname);
  }
});

console.log(`Peek simulator listening on http://localhost:${port}`);

function servePreviewFrame(pathname: string) {
  const fileName = decodeURIComponent(pathname.replace("/frames/", ""));
  if (!/^[a-zA-Z0-9._-]+$/.test(fileName)) {
    return new Response("Invalid frame path", { status: 400 });
  }
  const path = join(previewRoot, fileName);
  if (!existsSync(path)) {
    return new Response("Frame not found", { status: 404 });
  }
  return new Response(Bun.file(path), {
    headers: {
      "Cache-Control": "no-store",
      "Content-Type": "image/png"
    }
  });
}

function serveStatic(pathname: string) {
  const cleanPath = pathname === "/" ? "/index.html" : pathname;
  const path = normalize(join(distRoot, cleanPath));
  if (!path.startsWith(distRoot)) {
    return new Response("Invalid path", { status: 400 });
  }
  if (existsSync(path)) {
    return new Response(Bun.file(path), {
      headers: {
        "Content-Type": contentType(path)
      }
    });
  }
  return new Response(Bun.file(join(distRoot, "index.html")), {
    headers: {
      "Content-Type": "text/html; charset=utf-8"
    }
  });
}

function contentType(path: string) {
  const ext = extname(path);
  if (ext === ".html") return "text/html; charset=utf-8";
  if (ext === ".js") return "text/javascript; charset=utf-8";
  if (ext === ".css") return "text/css; charset=utf-8";
  if (ext === ".svg") return "image/svg+xml";
  return "application/octet-stream";
}
