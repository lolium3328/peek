# Peek Simulator

Non-invasive simulator workspace for Peek.

The simulator is split from the production web app and reads firmware rendering
code instead of rewriting the round-screen UI in the browser.

## Packages

- `protocol`: shared TypeScript render and state contracts.
- `native`: host-side C++ renderer that compiles firmware `ScreenRenderer.cpp`.
- `web`: browser controls plus a Bun render service.

## Render A Fixture

```bash
cd simulator/native
bun run render -- --input fixtures/home.json --out ../../.peek-preview/simulator-home.png
```

## Run The Web Simulator

```bash
cd simulator/web
bun install
bun run build
bun run start
```

Open the printed local URL.
