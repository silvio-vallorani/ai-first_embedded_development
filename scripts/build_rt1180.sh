#!/usr/bin/env bash
# Build the CM33 + CM7 LED-toggle application through the NXP SysBuild template.
#
# Usage: build_rt1180.sh [--board NAME] [--mcuboot] [--version X.Y.Z] [--toggle-period-us N] [--no-confirm]
#   --board NAME         Board under boards/: mimxrt1180_evk (default) or am254-4.
#   --mcuboot            Link for the MCUboot primary slot (ADR-0014) instead of direct ROM boot.
#   --version X.Y.Z      Version printed by the application (use the signed image version).
#   --toggle-period-us N LED toggle period in microseconds.
#   --no-confirm         MCUboot variant only: never confirm a test image (rollback test).
set -euo pipefail

BOARD="mimxrt1180_evk"
VARIANT="standalone"
APP_VERSION=""
TOGGLE_PERIOD_US=""
NO_CONFIRM=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --board) BOARD="${2:?--board needs a value}"; shift ;;
        --mcuboot) VARIANT="mcuboot" ;;
        --version) APP_VERSION="${2:?--version needs a value}"; shift ;;
        --toggle-period-us) TOGGLE_PERIOD_US="${2:?--toggle-period-us needs a value}"; shift ;;
        --no-confirm) NO_CONFIRM=1 ;;
        *) echo "Error: unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done
if [[ -n "${APP_VERSION}" && ! "${APP_VERSION}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Error: --version must be X.Y.Z (decimal numbers)." >&2
    exit 2
fi
if [[ -n "${TOGGLE_PERIOD_US}" && ! "${TOGGLE_PERIOD_US}" =~ ^[1-9][0-9]*$ ]]; then
    echo "Error: --toggle-period-us must be a positive integer." >&2
    exit 2
fi
if [[ "${NO_CONFIRM}" == 1 && "${VARIANT}" != "mcuboot" ]]; then
    echo "Error: --no-confirm requires --mcuboot." >&2
    exit 2
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
SDK_ROOT="${MCUXPRESSO_SDK:-${PROJECT_ROOT}/../mcuxpresso-sdk}"
SDK_EXAMPLES="${SDK_ROOT}/mcuxsdk/examples/multicore_examples"
TEMPLATE="${SDK_EXAMPLES}/hello_world"
OVERLAY="${SDK_EXAMPLES}/aied_dual_core_led_toggle"
EXAMPLE_DIR="${PROJECT_ROOT}/examples/dual_core_led_toggle"
BOARD_DIR="${PROJECT_ROOT}/boards/${BOARD}"
if [[ ! -f "${BOARD_DIR}/include/aied_board.h" ]]; then
    echo "Error: unknown board ${BOARD} (expected boards/<name>/include/aied_board.h)." >&2
    exit 2
fi
# Short build-directory prefix per board (Windows path-length limit, see the rt1180-build skill).
BUILD_PREFIX="rt1180"
[[ "${BOARD}" == "mimxrt1180_evk" ]] || BUILD_PREFIX="${BOARD}"
if [[ "${VARIANT}" == "mcuboot" ]]; then
    BUILD_DIR="${RT1180_BUILD_DIR:-${PROJECT_ROOT}/build/${BUILD_PREFIX}-led-mcuboot}"
else
    BUILD_DIR="${RT1180_BUILD_DIR:-${PROJECT_ROOT}/build/${BUILD_PREFIX}-led-toggle}"
fi
VENV_DIR="${PROJECT_ROOT}/.venv"
WEST_BIN="${VENV_DIR}/bin/west"
MARKER=".aied-generated-overlay"
TEMPLATE_IMAGE_PREFIX="hello_world"
APP_IMAGE_PREFIX="led_toggle"

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
cp "${EXAMPLE_DIR}/cm33/main.c" "${OVERLAY}/primary/main.c"
cp "${EXAMPLE_DIR}/cm7/main.c" "${OVERLAY}/secondary/main.c"
cp "${EXAMPLE_DIR}/common/app_events.h" "${OVERLAY}/primary/app_events.h"
cp "${EXAMPLE_DIR}/common/app_events.h" "${OVERLAY}/secondary/app_events.h"
cp "${BOARD_DIR}/include/aied_board.h" "${OVERLAY}/primary/aied_board.h"
cp "${BOARD_DIR}/include/aied_board.h" "${OVERLAY}/secondary/aied_board.h"
# A board without an SDK board port of its own replaces EVK board-port files of an image:
# pin_mux.[ch] and/or hardware_init.c + app.h. CUSTOM_PRJ_CONF_PATHS is merged after the SDK
# board-category prj.conf, which selects the EVK files.
for image in primary:cm33 secondary:cm7; do
    src="${BOARD_DIR}/sdk/${image#*:}"
    dst="${OVERLAY}/${image%%:*}"
    conf=""
    files=""
    if [[ -f "${src}/pin_mux.c" ]]; then
        conf+="CONFIG_MCUX_PRJSEG_module.board.pinmux_customize_folder=y\n"
        files+=" pin_mux.c pin_mux.h"
    fi
    if [[ -f "${src}/hardware_init.c" ]]; then
        conf+="CONFIG_MCUX_PRJSEG_project.hw_app_customize_folder=y\n"
        files+=" hardware_init.c app.h"
    fi
    [[ -n "${files}" ]] || continue
    (cd "${src}" && cp ${files} "${dst}/")
    printf "${conf}" > "${dst}/board_port.conf"
    perl -0pi -e 's/^(project\()/set(CUSTOM_PRJ_CONF_PATHS \$\{CMAKE_CURRENT_LIST_DIR\}\/board_port.conf)\n$1/m' "${dst}/CMakeLists.txt"
    grep -q '^set(CUSTOM_PRJ_CONF_PATHS' "${dst}/CMakeLists.txt"
    printf '\nmcux_add_source(\n    SOURCES%s\n)\n' "${files}" >> "${dst}/CMakeLists.txt"
done

test -f "${OVERLAY}/primary/sysbuild.cmake"
test -f "${OVERLAY}/secondary/CMakeLists.txt"

# Rename the template's image and IDE workspace names so build outputs carry this
# application's name. Board-port paths (multicore_examples/hello_world) are NXP-owned
# and stay unchanged.
TEMPLATE_NAMES="${TEMPLATE_IMAGE_PREFIX}_(primary|secondary)_core|shared-workspace: ${TEMPLATE_IMAGE_PREFIX}\$"
grep -rlE "${TEMPLATE_NAMES}" "${OVERLAY}" |
    xargs perl -pi -e "s/\b${TEMPLATE_IMAGE_PREFIX}_(primary|secondary)_core/${APP_IMAGE_PREFIX}_\$1_core/g;
                       s/(shared-workspace: )${TEMPLATE_IMAGE_PREFIX}\$/\${1}${APP_IMAGE_PREFIX}/"
if grep -rqE "${TEMPLATE_NAMES}" "${OVERLAY}" ||
    ! grep -q "APPLICATION ${APP_IMAGE_PREFIX}_secondary_core" "${OVERLAY}/primary/sysbuild.cmake"; then
    echo "Error: the NXP template image names could not be renamed." >&2
    exit 1
fi

# Logging service (both variants) and variant/build-time parameters (CM33 primary image only).
printf '\nset(AIED_ROOT "%s")\n' "${PROJECT_ROOT}" >> "${OVERLAY}/primary/CMakeLists.txt"
cat "${EXAMPLE_DIR}/log/primary.cmake" >> "${OVERLAY}/primary/CMakeLists.txt"
cat "${EXAMPLE_DIR}/log/primary.conf" >> "${OVERLAY}/primary/prj.conf"
if [[ "${VARIANT}" == "mcuboot" ]]; then
    cat "${EXAMPLE_DIR}/mcuboot/primary.cmake" >> "${OVERLAY}/primary/CMakeLists.txt"
    cat "${EXAMPLE_DIR}/mcuboot/primary.conf" >> "${OVERLAY}/primary/prj.conf"
else
    cat "${EXAMPLE_DIR}/standalone/primary.cmake" >> "${OVERLAY}/primary/CMakeLists.txt"
fi
APP_MACROS=""
if [[ -n "${APP_VERSION}" ]]; then
    IFS=. read -r VERSION_MAJOR VERSION_MINOR VERSION_PATCH <<< "${APP_VERSION}"
    APP_MACROS+=" -DAPP_VERSION_MAJOR=${VERSION_MAJOR} -DAPP_VERSION_MINOR=${VERSION_MINOR}"
    APP_MACROS+=" -DAPP_VERSION_PATCH=${VERSION_PATCH}"
fi
if [[ -n "${TOGGLE_PERIOD_US}" ]]; then
    APP_MACROS+=" -DAPP_TOGGLE_PERIOD_US=${TOGGLE_PERIOD_US}U"
fi
if [[ "${NO_CONFIRM}" == 1 ]]; then
    APP_MACROS+=" -DAIED_SKIP_IMAGE_CONFIRM=1"
fi
if [[ -n "${APP_MACROS}" ]]; then
    printf '\nmcux_add_macro(\n    CC "%s"\n)\n' "${APP_MACROS# }" >> "${OVERLAY}/primary/CMakeLists.txt"
fi

(
    cd "${SDK_ROOT}"
    "${WEST_BIN}" build -b evkmimxrt1180 --sysbuild \
        "${OVERLAY}/primary" \
        --config flexspi_nor_debug --toolchain armgcc \
        -Dcore_id=cm33 -DPython3_EXECUTABLE="${VENV_DIR}/bin/python" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -p always \
        -d "${BUILD_DIR}"
)

echo "Build completed (${BOARD}, ${VARIANT}): ${BUILD_DIR}"
