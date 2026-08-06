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
  provision-ota  Install the one-time OTA credential over USB

Optional: set Z2_PORT=/dev/cu.usbmodem... to select a specific board.
EOF
}

require_setup() {
  if [[ -z "$PIO_BIN" ]]; then
    echo "Error: PlatformIO 'pio' command not found." >&2
    exit 1
  fi
}

read_ota_password() {
  OTA_PASSWORD="${Z2_OTA_PASSWORD:-$(security find-generic-password -s com.kentwynn.z2.ota -a z2-001 -w 2>/dev/null || true)}"
  if [[ ${#OTA_PASSWORD} -lt 12 ]]; then
    echo "Error: set Z2_OTA_PASSWORD to the device-scoped OTA credential issued during onboarding." >&2
    exit 1
  fi
}

provision_ota() {
  resolve_port
  local generated_password pio_python existing_password provision_status
  existing_password="$(security find-generic-password -s com.kentwynn.z2.ota -a z2-001 -w 2>/dev/null || true)"
  generated_password="${existing_password:-$(openssl rand -hex 24)}"
  pio_python="$(sed -n '1s/^#!//p' "$PIO_BIN")"
  provision_status=0
  "$pio_python" "$PROJECT_DIR/scripts/provision_ota.py" \
    --port "$PORT" --password "$generated_password" || provision_status=$?
  if [[ $provision_status -eq 2 && -z "$existing_password" ]]; then
    echo "Error: Z2 already has an OTA password but this Mac has no matching Keychain entry." >&2
    echo "Use USB recovery to explicitly rotate it; refusing to overwrite automatically." >&2
    exit 1
  fi
  [[ $provision_status -eq 0 || $provision_status -eq 2 ]] || exit "$provision_status"
  security add-generic-password -U -s com.kentwynn.z2.ota -a z2-001 \
    -w "$generated_password" >/dev/null
  generated_password=""
  echo "OTA credential saved in macOS Keychain; future ./z2.sh ota needs no export."
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
    flash)
      resolve_port
      run_pio run -e z2 -t upload --upload-port "$PORT"
      provision_ota
      ;;
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
      "$pio_python" "$espota_script" -i "${Z2_OTA_HOST:-z2-001.local}" \
        -p 3232 -a "$OTA_PASSWORD" -f "$PROJECT_DIR/.pio/build/z2/firmware.bin" -r
      ;;
    provision-ota) provision_ota ;;
    help|-h|--help) usage ;;
    *) echo "Unknown command: $1" >&2; usage; exit 1 ;;
  esac
}

main "$@"
