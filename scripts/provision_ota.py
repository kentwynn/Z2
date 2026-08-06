#!/usr/bin/env python3
"""Install a one-time OTA credential over the physical USB serial link."""

import argparse
import sys
import time

import serial


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--password", required=True)
    args = parser.parse_args()
    deadline = time.monotonic() + 25
    with serial.Serial(args.port, 115200, timeout=0.25) as connection:
        time.sleep(1.5)
        connection.reset_input_buffer()
        connection.write(f"Z2_OTA_PROVISION {args.password}\n".encode())
        connection.flush()
        while time.monotonic() < deadline:
            line = connection.readline().decode("utf-8", errors="replace").strip()
            if "OTA_PROVISIONED" in line:
                print("Z2 confirmed OTA credential installation.")
                return 0
            if "OTA_ALREADY_PROVISIONED" in line:
                print("Z2 already has an OTA credential.", file=sys.stderr)
                return 2
            if "OTA_PROVISION_REJECTED" in line or "OTA_PROVISION_FAILED" in line:
                print("Z2 rejected OTA credential installation.", file=sys.stderr)
                return 1
    print("Timed out waiting for Z2 OTA provisioning acknowledgement.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
