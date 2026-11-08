#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
SDK_ROOT="${MCUXPRESSO_SDK:-${PROJECT_ROOT}/../mcuxpresso-sdk}"
SDK_EXAMPLES="${SDK_ROOT}/mcuxsdk/examples/multicore_examples"
TEMPLATE="${SDK_EXAMPLES}/hello_world"
OVERLAY="${SDK_EXAMPLES}/aied_dual_core_led_toggle"
BUILD_DIR="${RT1180_BUILD_DIR:-${PROJECT_ROOT}/build/rt1180-dual-core-led-toggle}"
VENV_DIR="${PROJECT_ROOT}/.venv"
WEST_BIN="${VENV_DIR}/bin/west"
MARKER=".aied-generated-overlay"

if [[ ! -x "${WEST_BIN}" ]]; then
    echo "Error: the project Python environment is missing. Run scripts/setup_sdk.sh first." >&2
    exit 1
fi
export PATH="${VENV_DIR}/bin:${PATH}"

for tool in cmake ninja; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "Error: ${tool} is required and must be on PATH." >&2
        exit 1
    fi
done

if [[ -n "${ARMGCC_DIR:-}" ]]; then
    export PATH="${ARMGCC_DIR}/bin:${PATH}"
fi
if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    echo "Error: Arm GNU Toolchain 14.2.x is required." >&2
    echo "Install it and set ARMGCC_DIR to the toolchain installation root." >&2
    exit 1
fi
if [[ ! -d "${SDK_ROOT}/.west" || ! -f "${TEMPLATE}/primary/CMakeLists.txt" ]]; then
    echo "Error: the pinned MCUXpresso SDK workspace is missing or incomplete." >&2
    echo "Run scripts/setup_sdk.sh first." >&2
    exit 1
fi

if [[ -e "${OVERLAY}" ]]; then
    if [[ ! -f "${OVERLAY}/${MARKER}" ]]; then
        echo "Error: refusing to overwrite unmarked directory: ${OVERLAY}" >&2
        exit 1
    fi
    rm -rf -- "${OVERLAY}"
fi

cp -a "${TEMPLATE}" "${OVERLAY}"
touch "${OVERLAY}/${MARKER}"
cp "${PROJECT_ROOT}/examples/dual_core_led_toggle/cm33/main.c" "${OVERLAY}/primary/main.c"
cp "${PROJECT_ROOT}/examples/dual_core_led_toggle/cm7/main.c" "${OVERLAY}/secondary/main.c"
cp "${PROJECT_ROOT}/examples/dual_core_led_toggle/common/app_events.h" "${OVERLAY}/primary/app_events.h"
cp "${PROJECT_ROOT}/examples/dual_core_led_toggle/common/app_events.h" "${OVERLAY}/secondary/app_events.h"

test -f "${OVERLAY}/primary/sysbuild.cmake"
test -f "${OVERLAY}/secondary/CMakeLists.txt"

(
    cd "${SDK_ROOT}"
    "${WEST_BIN}" build -b evkmimxrt1180 --sysbuild \
        "${OVERLAY}/primary" \
        --config flexspi_nor_debug --toolchain armgcc \
        -Dcore_id=cm33 -DPython3_EXECUTABLE="${VENV_DIR}/bin/python" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -p always \
        -d "${BUILD_DIR}"
)

echo "Build completed: ${BUILD_DIR}"
