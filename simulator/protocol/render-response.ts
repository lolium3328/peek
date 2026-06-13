import type { SimulatorState } from "./simulator-state";

export interface SimulatorRenderResponse {
  imageUrl: string;
  imagePath: string;
  width: number;
  height: number;
  renderedAt: number;
  state: SimulatorState;
}
