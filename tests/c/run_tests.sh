#!/usr/bin/env bash
# Build and run the host (native) C unit tests for the core/ library.
#
# Requires a Linux host (e.g. WSL2) with gcc and an ESP-IDF installation for
# the vendored Unity source. Activate the IDF environment first, e.g.:
#     . /home/grand/esp/esp-idf-v6.0/export.sh
#     tests/c/run_tests.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CORE="$ROOT/core"
TEST_SRC="$ROOT/tests/c/test_core.c"
OUT_DIR="$ROOT/build_host/tests-c"
mkdir -p "$OUT_DIR"

if [ -z "${IDF_PATH:-}" ]; then
    echo "error: IDF_PATH is not set. Run the ESP-IDF export.sh first (e.g. source \$IDF_PATH/export.sh)." >&2
    exit 1
fi
UNITY_SRC="$IDF_PATH/components/unity/unity"
if [ ! -f "$UNITY_SRC/src/unity.c" ]; then
    echo "error: Unity not found under \$IDF_PATH/components/unity/unity" >&2
    exit 1
fi

gcc -std=c11 -Wall -Wextra -Werror \
    -I "$CORE/include" \
    -I "$UNITY_SRC/src" \
    "$CORE/src/core_config.c" \
    "$CORE/src/core_display.c" \
    "$CORE/src/core_time.c" \
    "$UNITY_SRC/src/unity.c" \
    "$TEST_SRC" \
    -o "$OUT_DIR/test_core"

"$OUT_DIR/test_core"
