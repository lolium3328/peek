#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
firmware_dir="${repo_root}/firmware"

find_upload_port() {
  local port
  local candidates=(
    /dev/cu.usbmodem*
    /dev/cu.usbserial*
    /dev/cu.SLAB_USBtoUART*
    /dev/cu.wchusbserial*
    /dev/ttyACM0
  )

  for port in "${candidates[@]}"; do
    if [[ -e "${port}" ]]; then
      printf '%s\n' "${port}"
      return 0
    fi
  done

  return 1
}

upload_port="${PEEK_UPLOAD_PORT:-}"

if [[ -z "${upload_port}" ]]; then
  if ! upload_port="$(find_upload_port)"; then
    cat >&2 <<'EOF'
No upload port found.

Connect the ESP32-S3, then retry. On macOS the port usually looks like:
  /dev/cu.usbmodem*
  /dev/cu.usbserial*

You can also set it explicitly:
  PEEK_UPLOAD_PORT=/dev/cu.usbmodemXXXX ./scripts/upload-firmware.sh
EOF
    exit 1
  fi
fi

echo "Uploading firmware to ${upload_port}"

uvx --with pip --from platformio platformio run -d "${firmware_dir}" -t upload --upload-port "${upload_port}"
