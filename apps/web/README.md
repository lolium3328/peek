# Peek Web

Bun backend and TypeScript configuration UI for the Peek embedded device.

## Run

```bash
bun install
bun run build
bun run start
```

Open the LAN URL printed by the server on desktop or mobile.

The server uses plain HTTP by default. To enable local HTTPS, run
`PEEK_HTTPS=1 bun run start`; that creates a self-signed certificate in
`.peek-data/certs` on first start.

If Bun is not installed globally yet, prefix commands with `npx`, for example
`npx bun run start`.

## Scripts

- `bun run build`: type-check and build the web frontend.
- `bun run start`: serve the built frontend and WebSocket API.
- `bun run dev`: build once, then run the Bun server in hot mode.
- `bun run dev:client`: run Vite only, proxying API and WebSocket requests to the Bun server.
- `bun run dev:server`: run only the Bun server.

## API

- `GET /api/snapshot`: current config, latest device status, and LAN URLs.
- `GET /api/config`: current device config.
- `PATCH /api/config`: update config fields.
- `POST /api/config/reset`: reset config to defaults.
- `GET /api/status`: latest device status.
- `POST /api/device/status`: update latest device status.
- `POST /api/device/command`: forward a command to connected device WebSockets.

## Structure

- `server/index.ts`: Bun server entry, timers, WebSocket upgrade.
- `server/routes.ts`: HTTP API routes.
- `server/state.ts`: config persistence, device status, broadcast snapshots.
- `server/websocket.ts`: browser/device WebSocket message handling.
- `server/static.ts`: production frontend serving.
- `src/main.ts`: configuration UI.
- `src/shared.ts`: shared config/status types and normalization.
- `src/icons.ts`: Lucide icon registry.
