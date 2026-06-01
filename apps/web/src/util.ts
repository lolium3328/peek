export function byId<T extends HTMLElement>(id: string) {
  const element = document.getElementById(id);
  if (!element) throw new Error(`Missing #${id}`);
  return element as T;
}

export async function request<T>(url: string, init?: RequestInit) {
  const response = await fetch(url, init);
  const body = (await response.json()) as { data?: T; error?: { message?: string } };
  if (!response.ok) throw new Error(body.error?.message ?? "请求失败");
  return body.data as T;
}

export function wsUrl() {
  const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
  return `${protocol}//${window.location.host}/ws`;
}

export function formatBytes(value: number) {
  if (value < 1024) return `${value} B`;
  if (value < 1024 * 1024) return `${(value / 1024).toFixed(1)} KB`;
  return `${(value / 1024 / 1024).toFixed(1)} MB`;
}

export function escapeHtml(value: string) {
  return value.replace(/[&<>"']/g, (char) => {
    const map: Record<string, string> = { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" };
    return map[char] ?? char;
  });
}

export function errorMessage(error: unknown) {
  return error instanceof Error ? error.message : "失败";
}

export function degree(value: number | null) {
  return value === null ? "--" : `${Math.round(value)}°`;
}

export function timeLabel(timestamp: number) {
  return new Intl.DateTimeFormat("zh-CN", {
    month: "2-digit", day: "2-digit", hour: "2-digit", minute: "2-digit", second: "2-digit"
  }).format(timestamp);
}

export function storageLabel(freeBytes: number | null, totalBytes: number | null) {
  if (freeBytes === null || totalBytes === null) return "--";
  return `${formatBytes(freeBytes)} / ${formatBytes(totalBytes)}`;
}
