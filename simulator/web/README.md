# Peek Web Simulator

Standalone browser simulator for Peek.

The page owns only simulator controls and state editing. Screen output is a PNG
produced by `simulator/native`, which compiles the firmware renderer.

## Run

```bash
bun install
bun run build
bun run start
```

The server listens on `PEEK_SIM_PORT`, or `3201` by default.
