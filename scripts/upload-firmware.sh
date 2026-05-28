#!/usr/bin/env bash
set -euo pipefail

upload_port="${PEEK_UPLOAD_PORT:-/dev/ttyACM0}"

uvx --with pip --from platformio platformio run -t upload --upload-port "${upload_port}"
