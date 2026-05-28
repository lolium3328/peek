import { join, normalize } from "node:path";

export const port = Number(Bun.env.PORT ?? 3001);
export const distRoot = normalize(join(import.meta.dir, "..", "dist"));
export const dataRoot = normalize(join(import.meta.dir, "..", ".peek-data"));
export const configPath = normalize(Bun.env.PEEK_CONFIG ?? join(dataRoot, "device-config.json"));
export const httpsEnabled = Bun.env.PEEK_HTTPS !== "0";
export const tlsCertPath = normalize(Bun.env.PEEK_TLS_CERT ?? join(dataRoot, "certs", "peek-local.crt"));
export const tlsKeyPath = normalize(Bun.env.PEEK_TLS_KEY ?? join(dataRoot, "certs", "peek-local.key"));
