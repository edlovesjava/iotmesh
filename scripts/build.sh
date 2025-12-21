#!/usr/bin/env bash
set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
FIRMWARE_DIR="$PROJECT_DIR/firmware"
BUILD_DIR="$PROJECT_DIR/build"

ENVS=(pir led button button2 dht watcher gateway)

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

usage() {
    echo "Usage: $0 [node_type|all] [--version VERSION]"
    echo ""
    echo "Node types: ${ENVS[*]}"
    echo ""
    echo "Examples:"
    echo "  $0 pir                    # Build PIR firmware"
    echo "  $0 all                    # Build all firmware"
    echo "  $0 pir --version 1.2.0    # Build with version tag"
    exit 1
}

# Parse arguments
NODE_TYPE=""
VERSION="dev"

while [[ $# -gt 0 ]]; do
    case $1 in
        --version|-v)
            VERSION="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            NODE_TYPE="$1"
            shift
            ;;
    esac
done

[[ -z "$NODE_TYPE" ]] && NODE_TYPE="all"

cd "$FIRMWARE_DIR"

echo -e "${CYAN}======================================${NC}"
echo -e "${CYAN}  PlatformIO Build${NC}"
echo -e "${CYAN}  Version: $VERSION${NC}"
echo -e "${CYAN}======================================${NC}"
echo ""

# Determine which environments to build
if [[ "$NODE_TYPE" == "all" ]]; then
    BUILD_ENVS=("${ENVS[@]}")
    pio run
else
    BUILD_ENVS=("$NODE_TYPE")
    pio run -e "$NODE_TYPE"
fi

# Copy binaries to build/ with version
echo ""
echo -e "${CYAN}Copying binaries...${NC}"

mkdir -p "$BUILD_DIR"

for env in "${BUILD_ENVS[@]}"; do
    src=".pio/build/$env/firmware.bin"
    if [[ -f "$src" ]]; then
        dest_dir="$BUILD_DIR/$env"
        mkdir -p "$dest_dir"
        dest="$dest_dir/${env}_${VERSION}.bin"
        cp "$src" "$dest"

        # Get file size (works on both Linux/macOS and Git Bash)
        if [[ "$OSTYPE" == "darwin"* ]]; then
            size=$(stat -f%z "$dest")
        else
            size=$(stat -c%s "$dest")
        fi
        size_kb=$((size / 1024))
        echo -e "  ${GREEN}$env${NC} -> $dest ($size_kb KB)"
    fi
done

echo ""
echo -e "${GREEN}Build complete!${NC}"
