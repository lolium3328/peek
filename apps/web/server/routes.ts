import { existsSync, mkdirSync, unlinkSync, writeFileSync } from "node:fs";
import { extname, join } from "node:path";
import { assetFilesRoot } from "./config";
import { convertGifToPka } from "./gifConverter";
import {
  currentAssetManifest,
  currentConfig,
  currentStatus,
  deleteAsset,
  deviceSyncPayload,
  patchDeviceConfig,
  resetDeviceConfig,
  sendDeviceCommand,
  setPet2Asset,
  snapshot,
  upsertAsset,
  updateDeviceStatus
} from "./state";
import {
  normalizeDeviceCommand,
  type DeviceConfig,
  type DeviceStatus,
  type PetAsset
} from "../src/shared";

export async function handleApiRequest(req: Request, url: URL): Promise<Response | null> {
  if (url.pathname === "/api/health") {
    return Response.json({ data: { ok: true, serverTime: Date.now() } });
  }

  if (url.pathname === "/api/snapshot") {
    return Response.json({ data: snapshot() });
  }

  if (url.pathname === "/api/config" && req.method === "GET") {
    return Response.json({ data: currentConfig() });
  }

  if (url.pathname === "/api/config" && req.method === "PATCH") {
    return handleConfigPatch(req);
  }

  if (url.pathname === "/api/config/reset" && req.method === "POST") {
    return Response.json({ data: resetDeviceConfig() });
  }

  if (url.pathname === "/api/status" && req.method === "GET") {
    return Response.json({ data: currentStatus() });
  }

  if (url.pathname === "/api/assets" && req.method === "GET") {
    return Response.json({ data: currentAssetManifest() });
  }

  if (url.pathname === "/api/assets" && req.method === "POST") {
    return handleAssetUpload(req);
  }

  if (url.pathname.startsWith("/api/assets/files/") && req.method === "GET") {
    return handleAssetFile(url);
  }

  if (url.pathname.startsWith("/api/assets/") && url.pathname.endsWith("/pet2") && req.method === "POST") {
    return handlePet2Asset(url);
  }

  if (url.pathname.startsWith("/api/assets/") && req.method === "DELETE") {
    return handleAssetDelete(url);
  }

  if (url.pathname === "/api/device/status" && req.method === "POST") {
    return handleDeviceStatus(req);
  }

  if (url.pathname === "/api/device/sync" && req.method === "POST") {
    return handleDeviceSync(req);
  }

  if (url.pathname === "/api/device/command" && req.method === "POST") {
    return handleDeviceCommand(req);
  }

  if (url.pathname.startsWith("/api/")) {
    return Response.json(
      {
        error: {
          code: "NOT_FOUND",
          message: "API 不存在"
        }
      },
      { status: 404 }
    );
  }

  return null;
}

async function handleConfigPatch(req: Request) {
  const body = await readJsonObject(req);
  if (body instanceof Response) {
    return body;
  }

  return Response.json({ data: patchDeviceConfig(body as Partial<DeviceConfig>) });
}

async function handleDeviceStatus(req: Request) {
  const body = await readJsonObject(req);
  if (body instanceof Response) {
    return body;
  }

  return Response.json({ data: updateDeviceStatus(body as Partial<DeviceStatus>) });
}

async function handleDeviceCommand(req: Request) {
  const body = await readJsonObject(req);
  if (body instanceof Response) {
    return body;
  }

  const command = normalizeDeviceCommand(body);
  const delivered = sendDeviceCommand(command);
  return Response.json({ data: { command, delivered } });
}

async function handleAssetUpload(req: Request) {
  let form: FormData;
  try {
    form = await req.formData();
  } catch {
    return errorResponse("INVALID_FORM", "请求体需要是 multipart/form-data", 400);
  }

  const file = form.get("file");
  if (!(file instanceof File)) {
    return errorResponse("MISSING_FILE", "缺少文件", 400);
  }

  const now = Date.now();
  const id = assetId(String(form.get("id") ?? file.name), now);
  const originalExt = extname(file.name).toLowerCase();
  const storedName = `${id}${originalExt || ".bin"}`;
  const path = join(assetFilesRoot, storedName);
  mkdirSync(assetFilesRoot, { recursive: true });
  const bytes = Buffer.from(await file.arrayBuffer());
  writeFileSync(path, bytes);

  let asset: PetAsset;
  const isGif = originalExt === ".gif" || file.type === "image/gif";
  if (isGif) {
    let converted: ReturnType<typeof convertGifToPka>;
    try {
      converted = convertGifToPka(bytes);
    } catch (error) {
      return errorResponse("INVALID_GIF", error instanceof Error ? error.message : "GIF 解析失败", 400);
    }

    const packageName = `${id}.pka`;
    writeFileSync(join(assetFilesRoot, packageName), converted.packageBytes);
    asset = {
      id,
      name: nonEmptyString(form.get("name"), file.name),
      kind: "sprite",
      format: "gif",
      width: converted.deviceWidth,
      height: converted.deviceHeight,
      sourceWidth: converted.sourceWidth,
      sourceHeight: converted.sourceHeight,
      deviceWidth: converted.deviceWidth,
      deviceHeight: converted.deviceHeight,
      frames: converted.frameCount,
      fps: converted.fpsEstimate,
      durationMs: converted.durationMs,
      frameDelaysMs: converted.frameDelaysMs,
      size: file.size,
      encodedSize: converted.packageBytes.length,
      path: `/api/assets/files/${storedName}`,
      sourcePath: `/api/assets/files/${storedName}`,
      devicePath: `/api/assets/files/${packageName}`,
      deviceFormat: "pka-rgb565-rle",
      createdAt: now,
      updatedAt: now
    };
  } else {
    asset = {
      id,
      name: nonEmptyString(form.get("name"), file.name),
      kind: normalizeAssetKind(form.get("kind")),
      format: nonEmptyString(form.get("format"), originalExt.replace(".", "") || "binary"),
      width: 0,
      height: 0,
      frames: 1,
      fps: 1,
      size: file.size,
      path: `/api/assets/files/${storedName}`,
      createdAt: now,
      updatedAt: now
    };
  }

  return Response.json({ data: upsertAsset(asset) });
}

function handleAssetFile(url: URL) {
  const fileName = decodeURIComponent(url.pathname.replace("/api/assets/files/", ""));
  if (!/^[a-zA-Z0-9._-]+$/.test(fileName)) {
    return errorResponse("INVALID_ASSET_PATH", "资源路径无效", 400);
  }

  const path = join(assetFilesRoot, fileName);
  if (!existsSync(path)) {
    return errorResponse("ASSET_NOT_FOUND", "资源不存在", 404);
  }

  return new Response(Bun.file(path));
}

function handleAssetDelete(url: URL) {
  const assetId = decodeURIComponent(url.pathname.replace("/api/assets/", ""));
  const asset = currentAssetManifest().assets.find((item) => item.id === assetId);
  if (!asset) {
    return errorResponse("ASSET_NOT_FOUND", "资源不存在", 404);
  }

  deleteAssetFile(asset.path);
  if (asset.sourcePath) deleteAssetFile(asset.sourcePath);
  if (asset.devicePath) deleteAssetFile(asset.devicePath);

  return Response.json({ data: deleteAsset(assetId) });
}

function handlePet2Asset(url: URL) {
  const assetId = decodeURIComponent(
    url.pathname.replace("/api/assets/", "").replace(/\/pet2$/, "")
  );
  const asset = currentAssetManifest().assets.find((item) => item.id === assetId);
  if (!asset) {
    return errorResponse("ASSET_NOT_FOUND", "资源不存在", 404);
  }
  if (!asset.devicePath || asset.deviceFormat !== "pka-rgb565-rle") {
    return errorResponse("UNSUPPORTED_ASSET", "该资源没有设备动画包", 400);
  }

  return Response.json({ data: setPet2Asset(assetId) });
}

async function handleDeviceSync(req: Request) {
  const body = await readJsonObject(req);
  if (body instanceof Response) {
    return body;
  }

  return Response.json({
    data: deviceSyncPayload((body.status ?? {}) as Partial<DeviceStatus>)
  });
}

async function readJsonObject(req: Request) {
  try {
    const body = await req.json();
    if (!body || typeof body !== "object" || Array.isArray(body)) {
      return errorResponse("INVALID_JSON", "请求体需要是 JSON 对象", 400);
    }
    return body as Record<string, unknown>;
  } catch {
    return errorResponse("INVALID_JSON", "请求体需要是 JSON", 400);
  }
}

function errorResponse(code: string, message: string, status: number) {
  return Response.json(
    {
      error: {
        code,
        message
      }
    },
    { status }
  );
}

function assetId(value: string, timestamp: number) {
  const slug = value
    .toLowerCase()
    .replace(/\.[a-z0-9]+$/i, "")
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "")
    .slice(0, 42);

  return `${slug || "asset"}-${timestamp.toString(36)}`;
}

function nonEmptyString(value: FormDataEntryValue | null, fallback: string) {
  return typeof value === "string" && value.trim().length > 0 ? value.trim() : fallback;
}

function intFormValue(value: FormDataEntryValue | null, fallback: number, min: number, max: number) {
  const next = typeof value === "string" ? Math.round(Number(value)) : fallback;
  if (!Number.isFinite(next)) {
    return fallback;
  }
  return Math.min(Math.max(next, min), max);
}

function normalizeAssetKind(value: FormDataEntryValue | null) {
  return value === "image" || value === "package" ? value : "sprite";
}

function deleteAssetFile(assetPath: string) {
  if (!assetPath.startsWith("/api/assets/files/")) {
    return;
  }
  const fileName = decodeURIComponent(assetPath.replace("/api/assets/files/", ""));
  const path = join(assetFilesRoot, fileName);
  if (existsSync(path)) {
    unlinkSync(path);
  }
}
