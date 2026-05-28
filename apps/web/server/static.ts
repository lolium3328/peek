import { extname, join, normalize } from "node:path";

function mimeType(pathname: string) {
  const types: Record<string, string> = {
    ".css": "text/css; charset=utf-8",
    ".html": "text/html; charset=utf-8",
    ".js": "text/javascript; charset=utf-8",
    ".json": "application/json; charset=utf-8",
    ".svg": "image/svg+xml",
    ".txt": "text/plain; charset=utf-8"
  };

  return types[extname(pathname)] ?? "application/octet-stream";
}

export async function serveStatic(distRoot: string, pathname: string) {
  const safePath = normalize(join(distRoot, pathname === "/" ? "index.html" : pathname));

  if (!safePath.startsWith(distRoot)) {
    return new Response("Forbidden", { status: 403 });
  }

  const file = Bun.file(safePath);
  if (await file.exists()) {
    return new Response(file, {
      headers: {
        "Content-Type": mimeType(safePath)
      }
    });
  }

  const index = Bun.file(join(distRoot, "index.html"));
  if (await index.exists()) {
    return new Response(index, {
      headers: {
        "Content-Type": "text/html; charset=utf-8"
      }
    });
  }

  return Response.json(
    {
      error: {
        code: "BUILD_MISSING",
        message: "Run `bun run build` before `bun run start`."
      }
    },
    { status: 404 }
  );
}
