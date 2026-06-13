import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join, normalize } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { normalizeSimulatorState } from "../../protocol/simulator-state";
import type { SimulatorRenderRequest } from "../../protocol/render-request";
import type { SimulatorRenderResponse } from "../../protocol/render-response";

const serverDir = dirname(fileURLToPath(import.meta.url));
const webRoot = normalize(join(serverDir, ".."));
const repoRoot = normalize(join(webRoot, "..", ".."));
const nativeRoot = join(repoRoot, "simulator", "native");
const previewRoot = join(repoRoot, ".peek-preview");

export async function handleRenderRequest(req: Request) {
  let body: SimulatorRenderRequest;
  try {
    body = await req.json() as SimulatorRenderRequest;
  } catch {
    return errorResponse("INVALID_JSON", "Request body must be JSON", 400);
  }

  const state = normalizeSimulatorState(body.state);
  const renderedAt = Date.now();
  const inputPath = join(previewRoot, "simulator-web-state.json");
  const fileName = "simulator-web.png";
  const outputPath = join(previewRoot, fileName);
  mkdirSync(previewRoot, { recursive: true });
  writeFileSync(inputPath, `${JSON.stringify(state, null, 2)}\n`, "utf8");

  const result = spawnSync(process.execPath, [
    "run",
    "render",
    "--",
    "--input",
    inputPath,
    "--out",
    outputPath
  ], {
    cwd: nativeRoot,
    encoding: "utf8"
  });

  if (result.status !== 0) {
    return Response.json({
      error: {
        code: "RENDER_FAILED",
        message: result.stderr.trim() || result.stdout.trim() || "Native renderer failed"
      }
    }, { status: 500 });
  }

  const data: SimulatorRenderResponse = {
    imageUrl: `/frames/${fileName}?t=${renderedAt}`,
    imagePath: outputPath,
    width: 240,
    height: 240,
    renderedAt,
    state
  };
  return Response.json({ data });
}

function errorResponse(code: string, message: string, status: number) {
  return Response.json({ error: { code, message } }, { status });
}
