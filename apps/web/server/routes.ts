import {
  currentConfig,
  currentStatus,
  patchDeviceConfig,
  resetDeviceConfig,
  sendDeviceCommand,
  snapshot,
  updateDeviceStatus
} from "./state";
import { normalizeDeviceCommand, type DeviceConfig, type DeviceStatus } from "../src/shared";

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

  if (url.pathname === "/api/device/status" && req.method === "POST") {
    return handleDeviceStatus(req);
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
