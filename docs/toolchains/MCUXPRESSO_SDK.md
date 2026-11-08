# MCUXpresso SDK setup

## Pinned baseline

| Item | Selected version |
|---|---|
| MCUXpresso SDK | 26.09.00 |
| Manifest repository | `nxp-mcuxpresso/mcuxsdk-manifests` |
| Manifest tag | `v26.09.00` |
| Reference board | `evkmimxrt1180` |
| MCU | MIMXRT1189 |
| Arm GNU Toolchain | 14.2.x |
| West | 1.4.0 (pinned in `requirements.txt`) |
| Python environment | Project-local `.venv` |
| Build orchestration | West + CMake + Ninja + SysBuild |

The SDK is an external dependency. Do not copy the entire SDK into this repository or follow its moving `main` branch for a reproducible build.

## Prerequisites

Install the following host tools:

- Git
- Python 3.8 or newer
- CMake
- Ninja
- Arm GNU Toolchain 14.2.x (Arm GNU Toolchain 14.2.rel1)

West (pinned to 1.4.0) and its required Python dependencies are installed from `requirements.txt` into the repository's `.venv`. No global `pip install`, `pip --user`, or manual virtual-environment activation is required. CMake, Ninja, Git and the Arm GNU Toolchain remain system-level tools.

Use the [NXP installation guide for SDK 26.09.00](https://docs.mcuxpresso.nxp.com/mcuxsdk/26.09.00/gsd/installation.html) for host-specific packages.

## Recommended VS Code workflow

1. Clone the project and open the repository root in VS Code.
2. Install the recommended extensions when VS Code prompts you.
3. Install Arm GNU Toolchain 14.2.rel1 and add its `bin` directory to `PATH`.
4. Set `ARMGCC_DIR` to the toolchain installation root (the directory containing `bin`).
5. Run **Terminal → Run Task → Setup MCUXpresso SDK**. This creates `.venv`, installs the pinned West package, initializes the manifest at `v26.09.00`, and fetches the repositories required by the EVK.
6. Run **Terminal → Run Task → Build RT1180 dual-core firmware**.

The SDK workspace defaults to a sibling directory named `mcuxpresso-sdk`. To use another location, set `MCUXPRESSO_SDK` to the directory that contains the West `.west` folder. Restart VS Code after changing persistent environment variables so its integrated terminal inherits them.

The scripts are available for both Windows PowerShell and Linux/macOS Bash:

| Purpose | Windows | Linux / macOS |
|---|---|---|
| Initialize Python environment and SDK | `scripts/setup_sdk.ps1` | `scripts/setup_sdk.sh` |
| Build CM33 + CM7 | `scripts/build_rt1180.ps1` | `scripts/build_rt1180.sh` |

The build scripts invoke West directly from `.venv`; you do not need to activate it manually. VS Code is configured to use the same interpreter for Python tooling. If you delete `.venv`, rerun the setup task to recreate it.

The setup script initializes the manifest at `v26.09.00`, resolves it, and runs `west update_board --set board evkmimxrt1180`. This targeted update avoids cloning every repository in the full SDK manifest.

Example PowerShell environment setup (adjust paths to the installed toolchain):

```powershell
[Environment]::SetEnvironmentVariable("ARMGCC_DIR", "C:\NXP\arm-gnu-toolchain-14.2.rel1", "User")
[Environment]::SetEnvironmentVariable("MCUXPRESSO_SDK", "D:\work\mcuxpresso-sdk", "User")
```

Example Bash environment setup:

```sh
export ARMGCC_DIR="$HOME/toolchains/arm-gnu-toolchain-14.2.rel1"
export MCUXPRESSO_SDK="$HOME/work/mcuxpresso-sdk"
export PATH="$ARMGCC_DIR/bin:$PATH"
```

## What the embedded build does

The build script creates a generated project overlay inside the SDK workspace at:

```text
mcuxsdk/examples/multicore_examples/aied_dual_core_led_toggle/
```

It copies NXP's `hello_world` multicore SysBuild template, replaces the CM33 and CM7 application sources and the shared `app_events.h`, and renames the template image names (`hello_world_primary_core` / `hello_world_secondary_core`) and IDE workspace name to `led_toggle_*`. The script stops if any template image name remains. NXP board support, linker scripts, startup, image-copy and packaging configuration remain sourced from the pinned SDK; the board-port path `multicore_examples/hello_world` is NXP-owned and is not renamed.

Project build outputs:

| Image | Path under `build/rt1180-led-toggle/` |
|---|---|
| CM33 (primary) | `primary/led_toggle_primary_core_cm33.elf` |
| CM7 (secondary) | `led_toggle_secondary_core/led_toggle_secondary_core_cm7.elf` |
| CM7 image embedded in CM33 | `led_toggle_secondary_core/armgcc/core1_image.bin` |

On Windows, the Arm GNU Toolchain compiler does not support paths longer than 260 characters even when Windows long paths are enabled. The deepest build file (`-fstack-usage` output under `led_toggle_secondary_core/CMakeFiles/...`) is about 165 characters below the build directory, so keep the build directory path under about 90 characters. Otherwise the build fails with `cannot open ... .su for writing: No such file or directory`. Clone the repository into a short path or set `RT1180_BUILD_DIR` to a short directory.

The generated overlay is marked with `.aied-generated-overlay`. The script refuses to delete or replace a directory at that path unless the marker exists. The firmware build output is written to `build/rt1180-led-toggle` in this repository (override with `RT1180_BUILD_DIR`).

The build enables CMake compilation database export when supported by the SDK. VS Code C/C++ IntelliSense is configured to use that database and to browse the project and SDK source trees. If IntelliSense cannot find SDK headers before the first embedded build, run the build task once and verify that `MCUXPRESSO_SDK` is set in the VS Code process environment.

## Manual setup

The setup scripts are the recommended way to create the Python environment and initialize the SDK. If you need to do it manually, run these commands from the repository root.

Linux/macOS:

```sh
PROJECT_ROOT="$(pwd)"
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/west init -m https://github.com/nxp-mcuxpresso/mcuxsdk-manifests.git \
  ../mcuxpresso-sdk --mr v26.09.00
cd ../mcuxpresso-sdk
"$PROJECT_ROOT/.venv/bin/west" manifest --resolve
"$PROJECT_ROOT/.venv/bin/west" update_board --set board evkmimxrt1180
"$PROJECT_ROOT/.venv/bin/west" list
```

On Windows PowerShell, use:

```powershell
$ProjectRoot = (Get-Location).Path
py -3 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
.\.venv\Scripts\west.exe init -m https://github.com/nxp-mcuxpresso/mcuxsdk-manifests.git ..\mcuxpresso-sdk --mr v26.09.00
Push-Location ..\mcuxpresso-sdk
try {
    & "$ProjectRoot\.venv\Scripts\west.exe" manifest --resolve
    & "$ProjectRoot\.venv\Scripts\west.exe" update_board --set board evkmimxrt1180
    & "$ProjectRoot\.venv\Scripts\west.exe" list
}
finally {
    Pop-Location
}
```

## Build the upstream dual-core reference example

From the MCUXpresso SDK workspace root, use the West executable in the project environment. If the project root is stored in `PROJECT_ROOT`:

```sh
"$PROJECT_ROOT/.venv/bin/west" build -b evkmimxrt1180 --sysbuild \
  mcuxsdk/examples/multicore_examples/hello_world/primary \
  --config flexspi_nor_debug --toolchain armgcc \
  -Dcore_id=cm33 -p always
```

## Host unit tests

The embedded West build is separate from the repository's host CMake presets. To build and run host tests:

```sh
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug
```

## Build validation

The GitHub Actions workflow `RT1189 embedded build` validates two distinct outputs:

1. The unmodified NXP CM33 + CM7 multicore baseline.
2. This repository's CM33 + CM7 dual-core LED-toggle application, built through the NXP SysBuild project template.

The workflow uses the same pinned West requirement and setup script as the local VS Code workflow, then uploads generated ELF, BIN, HEX and MAP files as a build artifact. A successful CI build confirms compilation and image packaging; it does not replace a physical EVK test of the LED, inter-core timing or boot behaviour.

NXP documentation:
- SDK 26.09.00 release: https://github.com/nxp-mcuxpresso/mcuxsdk-manifests/releases/tag/v26.09.00
- MIMXRT1180-EVK board page: https://docs.mcuxpresso.nxp.com/mcuxsdk/26.09.00/boards/RT/evkmimxrt1180/index.html
- Repository setup: https://docs.mcuxpresso.nxp.com/mcuxsdk/26.09.00/gsd/repo_setup.html
- SysBuild: https://docs.mcuxpresso.nxp.com/mcuxsdk/26.09.00/develop/build_system/Sysbuild.html
