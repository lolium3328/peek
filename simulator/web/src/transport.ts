import type { SimulatorRenderRequest } from "../../protocol/render-request";
import type { SimulatorRenderResponse } from "../../protocol/render-response";

interface ApiEnvelope<T> {
  data?: T;
  error?: {
    code: string;
    message: string;
  };
}

export async function renderSimulatorFrame(request: SimulatorRenderRequest) {
  const response = await fetch("/api/render", {
    method: "POST",
    headers: {
      "Content-Type": "application/json"
    },
    body: JSON.stringify(request)
  });
  const body = await response.json() as ApiEnvelope<SimulatorRenderResponse>;
  if (!response.ok || !body.data) {
    throw new Error(body.error?.message ?? `Render failed with ${response.status}`);
  }
  return body.data;
}
