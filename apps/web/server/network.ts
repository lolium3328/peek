import { networkInterfaces } from "node:os";

export type ServerProtocol = "http" | "https";

export function localIPv4Addresses() {
  const addresses = new Set<string>();

  for (const entries of Object.values(networkInterfaces())) {
    for (const entry of entries ?? []) {
      if (entry.family === "IPv4" && !entry.internal) {
        addresses.add(entry.address);
      }
    }
  }

  return Array.from(addresses);
}

export function lanUrls(port: number, protocol: ServerProtocol) {
  const urls = new Set<string>([`${protocol}://localhost:${port}`]);

  for (const address of localIPv4Addresses()) {
    urls.add(`${protocol}://${address}:${port}`);
  }

  return Array.from(urls);
}
