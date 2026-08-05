#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PIO_BIN="${Z2_PIO_BIN:-$(command -v pio || true)}"
PORT="${Z2_PORT:-}"
BAUD="115200"

usage() {
  cat <<'EOF'
Usage: ./z2.sh <command>

Commands:
  build          Compile the Z2 two-wheel controller
  flash          Upload the Z2 controller
  monitor        Open the Z2 serial monitor
  flash-monitor  Upload, then open the serial monitor
  ota            Build and upload wirelessly to z2.local

Optional: set Z2_PORT=/dev/cu.usbmodem... to select a specific board.
EOF
}

require_setup() {
  if [[ -z "$PIO_BIN" ]]; then
    echo "Error: PlatformIO 'pio' command not found." >&2
    exit 1
  fi
  if [[ ! -f "$PROJECT_DIR/include/wifi_credentials.h" ]]; then
    echo "Error: Z2 Wi-Fi credentials are missing." >&2
    echo "Copy include/wifi_credentials.example.h to include/wifi_credentials.h and edit it." >&2
    exit 1
  fi
  if [[ ! -f "$PROJECT_DIR/include/ota_credentials.h" ]]; then
    echo "Error: Z2 OTA credentials are missing." >&2
    echo "Copy include/ota_credentials.example.h to include/ota_credentials.h and edit it." >&2
    exit 1
  fi
}

read_ota_password() {
  local credentials="$PROJECT_DIR/include/ota_credentials.h"
  OTA_PASSWORD="$(awk -F'"' '/Z2_OTA_PASSWORD/ { print $2; exit }' "$credentials")"
  if [[ -z "$OTA_PASSWORD" || "$OTA_PASSWORD" == "SET_YOUR_OTA_PASSWORD" || ${#OTA_PASSWORD} -lt 8 ]]; then
    echo "Error: set a private OTA password of at least 8 characters in include/ota_credentials.h." >&2
    exit 1
  fi
}

resolve_port() {
  if [[ -n "$PORT" && -e "$PORT" ]]; then
    return
  fi

  local candidates=()
  local pattern dev
  for pattern in /dev/cu.usbmodem* /dev/cu.usbserial*; do
    for dev in $pattern; do
      [[ -e "$dev" ]] && candidates+=("$dev")
    done
  done

  if [[ ${#candidates[@]} -eq 0 ]]; then
    echo "Error: no USB serial device detected." >&2
    exit 1
  fi
  if [[ ${#candidates[@]} -gt 1 ]]; then
    echo "Error: multiple boards detected. Set Z2_PORT to the Z2 device:" >&2
    printf '  %s\n' "${candidates[@]}" >&2
    exit 1
  fi
  PORT="${candidates[0]}"
  echo "Using Z2 serial port: $PORT"
}

run_pio() {
  cd "$PROJECT_DIR"
  "$PIO_BIN" "$@"
}

main() {
  [[ $# -ge 1 ]] || { usage; exit 1; }
  require_setup
  case "$1" in
    build) run_pio run -e z2 ;;
    flash) resolve_port; run_pio run -e z2 -t upload --upload-port "$PORT" ;;
    monitor) resolve_port; run_pio device monitor -b "$BAUD" --port "$PORT" ;;
    flash-monitor)
      resolve_port
      run_pio run -e z2 -t upload --upload-port "$PORT"
      run_pio device monitor -b "$BAUD" --port "$PORT"
      ;;
    ota)
      read_ota_password
      run_pio run -e z2
      local pio_python espota_script
      pio_python="$(sed -n '1s/^#!//p' "$PIO_BIN")"
      espota_script="$HOME/.platformio/packages/framework-arduinoespressif32/tools/espota.py"
      [[ -x "$pio_python" && -f "$espota_script" ]] || {
        echo "Error: PlatformIO OTA uploader not found." >&2
        exit 1
      }
      "$pio_python" "$espota_script" -i "${Z2_OTA_HOST:-z2.local}" \
        -p 3232 -a "$OTA_PASSWORD" -f "$PROJECT_DIR/.pio/build/z2/firmware.bin" -r
      ;;
    help|-h|--help) usage ;;
    *) echo "Unknown command: $1" >&2; usage; exit 1 ;;
  esac
}

main "$@"
