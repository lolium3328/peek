# Peek

Peek is a firmware-first monorepo for a round-screen ESP32-S3 pet device.

## Layout

- `platformio.ini`, `src/`, `include/`: embedded firmware.
- `include/assets`: firmware assets such as generated fonts.
- `apps/web`: Bun backend and TypeScript configuration UI.
- `docs/hardware`: hardware datasheets and reference material.

## Firmware

```bash
uvx --with pip --from platformio platformio run
scripts/upload-firmware.sh
```

`scripts/upload-firmware.sh` defaults to `/dev/ttyACM0`. Override it with
`PEEK_UPLOAD_PORT=/dev/ttyUSB0 scripts/upload-firmware.sh` when needed.

## Web Config Service

```bash
cd apps/web
bun install
bun run build
bun run start
```

Set `PEEK_HTTPS=0` when plain HTTP is enough during local debugging.

The server is the primary control plane. Browsers talk to the Bun service, and
the ESP32 talks back to the service from STA mode through `/api/device/sync`.
The device caches the latest layout and asset manifest in LittleFS so it can
boot with the last known screen setup even when the server is unavailable.

- Layouts are saved by `PUT /api/layout` and previewed by `POST /api/layout/preview`.
- Animation assets are uploaded to `POST /api/assets` and exposed through the
  returned manifest.
- Firmware stores large editable data in LittleFS and keeps small calibration
  values in NVS/Preferences.

## First Setup

When no Wi-Fi SSID is saved, the firmware starts a setup access point:

- SSID: `Peek-xxxx`
- Password: `peeksetup`
- Setup page: `http://192.168.4.1`

Save the Wi-Fi SSID, Wi-Fi password, backend URL, and device identity there.
Peek writes those values to NVS and restarts into STA mode. Hold the button
while booting to force setup mode without erasing the saved values.
