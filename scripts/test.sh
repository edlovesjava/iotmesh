#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIRMWARE_DIR="$(dirname "$SCRIPT_DIR")/firmware"

ENVS=(pir led button button2 dht watcher gateway)

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'

cd "$FIRMWARE_DIR"

echo -e "${CYAN}======================================${NC}"
echo -e "${CYAN}  PlatformIO Compile Test${NC}"
echo -e "${CYAN}======================================${NC}"
echo ""

passed=0
failed=0

for env in "${ENVS[@]}"; do
    printf "  Compiling: %-12s ... " "$env"

    if pio run -e "$env" -s > /dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        ((passed++))
    else
        echo -e "${RED}FAIL${NC}"
        ((failed++))
    fi
done

echo ""
echo -e "${CYAN}======================================${NC}"
echo -e "  Passed: ${GREEN}$passed${NC}"
[[ $failed -gt 0 ]] && echo -e "  Failed: ${RED}$failed${NC}"
echo -e "${CYAN}======================================${NC}"

[[ $failed -gt 0 ]] && exit 1
exit 0
