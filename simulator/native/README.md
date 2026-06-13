# Peek Native Simulator

Host-side renderer for Peek simulator frames.

The runner compiles the firmware `ScreenRenderer.cpp` with host adapters and
renders JSON snapshots into PNG files. It is intentionally outside the firmware
tree: firmware rendering stays authoritative, while simulator code provides the
desktop-only adapters.

## Render

```bash
bun run render -- --input fixtures/home.json --out ../../.peek-preview/simulator-home.png
```

The command writes a 240x240 PNG using the real firmware screen renderer.
