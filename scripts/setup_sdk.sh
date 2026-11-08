#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
SDK_ROOT="${MCUXPRESSO_SDK:-${PROJECT_ROOT}/../mcuxpresso-sdk}"
VENV_DIR="${PROJECT_ROOT}/.venv"
VENV_PYTHON="${VENV_DIR}/bin/python"
WEST_BIN="${VENV_DIR}/bin/west"
SDK_TAG="v26.09.00"
BOARD="evkmimxrt1180"

for tool in git cmake ninja; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "Error: ${tool} is required and must be on PATH." >&2
        exit 1
    fi
done

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

if [[ ! -x "${VENV_PYTHON}" ]]; then
    if [[ -e "${VENV_DIR}" ]]; then
        echo "Error: ${VENV_DIR} exists but is not a valid virtual environment." >&2
        echo "Remove that directory and run this setup script again." >&2
        exit 1
    fi
    "${PYTHON_BIN}" -m venv "${VENV_DIR}"
fi

"${VENV_PYTHON}" -m pip install --disable-pip-version-check -r "${PROJECT_ROOT}/requirements.txt"
if [[ ! -x "${WEST_BIN}" ]]; then
    echo "Error: West was not installed in the project virtual environment." >&2
    exit 1
fi
export PATH="${VENV_DIR}/bin:${PATH}"
"${WEST_BIN}" --version

mkdir -p "$(dirname -- "${SDK_ROOT}")"
if [[ ! -d "${SDK_ROOT}/.west" ]]; then
    if [[ -n "$(find "${SDK_ROOT}" -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null)" ]]; then
        echo "Error: ${SDK_ROOT} exists and is not an initialized West workspace." >&2
        echo "Choose an empty SDK directory with MCUXPRESSO_SDK." >&2
        exit 1
    fi
    rmdir "${SDK_ROOT}" 2>/dev/null || true
    "${WEST_BIN}" init -m https://github.com/nxp-mcuxpresso/mcuxsdk-manifests.git \
        "${SDK_ROOT}" --mr "${SDK_TAG}"
fi

(
    cd "${SDK_ROOT}"
    "${WEST_BIN}" manifest --resolve >/dev/null
    "${WEST_BIN}" update_board --set board "${BOARD}"
    "${WEST_BIN}" list
)

echo
echo "Python environment ready: ${VENV_DIR}"
echo "West version: $("${WEST_BIN}" --version)"
echo "MCUXpresso SDK workspace ready: ${SDK_ROOT}"
echo "Set MCUXPRESSO_SDK to this directory if you use a different location."
echo "Install Arm GNU Toolchain 14.2.x and set ARMGCC_DIR to its installation root."
echo "Then run the VS Code task: Build RT1180 dual-core firmware."
