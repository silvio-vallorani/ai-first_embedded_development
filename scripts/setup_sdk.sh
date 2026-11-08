#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
SDK_ROOT="${MCUXPRESSO_SDK:-${PROJECT_ROOT}/../mcuxpresso-sdk}"
SDK_TAG="v26.09.00"
BOARD="evkmimxrt1180"

if ! command -v git >/dev/null 2>&1; then
    echo "Error: Git is required." >&2
    exit 1
fi

PYTHON_BIN="${PYTHON:-}"
if [[ -z "${PYTHON_BIN}" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        echo "Error: Python 3.8 or newer is required." >&2
        exit 1
    fi
fi

"${PYTHON_BIN}" -c 'import sys; raise SystemExit(0 if sys.version_info >= (3, 8) else 1)' || {
    echo "Error: Python 3.8 or newer is required." >&2
    exit 1
}

if ! command -v cmake >/dev/null 2>&1 || ! command -v ninja >/dev/null 2>&1; then
    echo "Error: install CMake and Ninja before continuing." >&2
    exit 1
fi

if ! command -v west >/dev/null 2>&1; then
    "${PYTHON_BIN}" -m pip install --user west
    USER_BASE="$("${PYTHON_BIN}" -c 'import site; print(site.USER_BASE)')"
    export PATH="${USER_BASE}/bin:${PATH}"
fi

if ! command -v west >/dev/null 2>&1; then
    echo "Error: west was installed but is not available on PATH." >&2
    echo "Add the Python user scripts directory to PATH and run this script again." >&2
    exit 1
fi

mkdir -p "$(dirname -- "${SDK_ROOT}")"
if [[ ! -d "${SDK_ROOT}/.west" ]]; then
    if [[ -n "$(find "${SDK_ROOT}" -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null)" ]]; then
        echo "Error: ${SDK_ROOT} exists and is not an initialized West workspace." >&2
        echo "Choose an empty SDK directory with MCUXPRESSO_SDK." >&2
        exit 1
    fi
    rmdir "${SDK_ROOT}" 2>/dev/null || true
    west init -m https://github.com/nxp-mcuxpresso/mcuxsdk-manifests.git \
        "${SDK_ROOT}" --mr "${SDK_TAG}"
fi

(
    cd "${SDK_ROOT}"
    west manifest --resolve >/dev/null
    west update_board --set board "${BOARD}"
    west list
)

echo
echo "MCUXpresso SDK workspace ready: ${SDK_ROOT}"
echo "Set MCUXPRESSO_SDK to this directory if you use a different location."
echo "Install Arm GNU Toolchain 14.2.x and set ARMGCC_DIR to its installation root."
echo "Then run the VS Code task: Build RT1180 dual-core firmware."
