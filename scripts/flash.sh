#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIRMWARE_DIR="$(dirname "$SCRIPT_DIR")/firmware"

usage() {
    echo "Usage: $0 <node_type> [port]"
    echo ""
    echo "Examples:"
    echo "  $0 pir              # Auto-detect port"
    echo "  $0 pir /dev/ttyUSB0 # Specify port (Linux)"
    echo "  $0 led COM3         # Specify port (Windows)"
    exit 1
}

[[ -z "$1" ]] && usage

NODE_TYPE="$1"
PORT="$2"

cd "$FIRMWARE_DIR"

if [[ -n "$PORT" ]]; then
    pio run -e "$NODE_TYPE" -t upload --upload-port "$PORT"
else
    pio run -e "$NODE_TYPE" -t upload
fi

echo ""
echo "Flash complete. Starting monitor..."
pio device monitor
