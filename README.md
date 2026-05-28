# Peek

Peek is a firmware-first monorepo for a round-screen ESP32-S3 pet device.

## Layout

- `platformio.ini`, `src/`, `include/`, `front/`: embedded firmware.
- `apps/web`: Bun backend and TypeScript configuration UI.
- `docs/`: product and protocol notes when needed.

## Firmware

```bash
uvx --with pip --from platformio platformio run
```

## Web Config Service

```bash
cd apps/web
bun install
bun run build
bun run start
```

Set `PEEK_HTTPS=0` when plain HTTP is enough during local debugging.
