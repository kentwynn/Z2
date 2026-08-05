from pathlib import Path

Import("env")


# ArduinoWebsockets 0.5.4 hardcodes a one-second wait for every HTTP upgrade
# header. Cloudflare can occasionally exceed that even after the API accepts
# the socket, causing the ESP32 to close a healthy handshake. Patch the fetched
# dependency deterministically before PlatformIO compiles it.
config = (
    Path(env.subst("$PROJECT_LIBDEPS_DIR"))
    / env.subst("$PIOENV")
    / "ArduinoWebsockets"
    / "src"
    / "tiny_websockets"
    / "ws_config_defs.hpp"
)
if not config.exists():
    raise RuntimeError(f"ArduinoWebsockets configuration not found: {config}")

source = config.read_text()
expected = "#define _CONNECTION_TIMEOUT 1000"
replacement = "#define _CONNECTION_TIMEOUT 5000"
if expected in source:
    config.write_text(source.replace(expected, replacement))
elif replacement not in source:
    raise RuntimeError("Unexpected ArduinoWebsockets timeout configuration")
