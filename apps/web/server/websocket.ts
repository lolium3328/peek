import type { ServerWebSocket } from "bun";
import {
  isClientMessage,
  type ClientMessage,
  type DeviceStatus
} from "../src/shared";
import {
  addClient,
  markDeviceConnected,
  patchDeviceConfig,
  previewLayout,
  removeClient,
  sendSnapshot,
  updateDeviceStatus
} from "./state";
import type { ClientData } from "./types";

export const websocketHandlers = {
  open(ws: ServerWebSocket<ClientData>) {
    addClient(ws);
    sendSnapshot(ws);
  },
  message(ws: ServerWebSocket<ClientData>, rawMessage: string | Buffer) {
    let message: ClientMessage;

    try {
      message = JSON.parse(String(rawMessage)) as ClientMessage;
    } catch {
      ws.send(JSON.stringify({ type: "notice", message: "Invalid JSON" }));
      return;
    }

    if (!isClientMessage(message)) {
      ws.send(JSON.stringify({ type: "notice", message: "Unknown message" }));
      return;
    }

    if (message.type === "browser.hello") {
      ws.data.role = "browser";
      sendSnapshot(ws);
      return;
    }

    if (message.type === "device.hello") {
      ws.data.role = "device";
      markDeviceConnected(message.status as Partial<DeviceStatus> | undefined);
      return;
    }

    if (message.type === "config.patch") {
      patchDeviceConfig(message.patch);
      return;
    }

    if (message.type === "layout.preview") {
      previewLayout(message.layout);
      return;
    }

    if (message.type === "device.status") {
      ws.data.role = "device";
      updateDeviceStatus(message.status);
    }
  },
  close(ws: ServerWebSocket<ClientData>) {
    removeClient(ws);
  }
};
