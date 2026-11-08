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
| Build orchestration | West + CMake + Ninja + SysBuild |

The SDK is an external dependency. Do not copy the entire SDK into this repository or follow its moving `main` branch for a reproducible build.

## Prerequisites

Install the versions documented by NXP for SDK 26.09.00:

- Git
- Python 3.8 or newer
- West
- CMake and Ninja
- Arm GNU Toolchain 14.2.x

Use the NXP installation guide for the exact host-specific packages and environment setup:
https://docs.mcuxpresso.nxp.com/mcuxsdk/26.09.00/gsd/installation.html

## Initialize a pinned SDK workspace

Run these commands outside this repository, for example from your firmware workspace directory:

```sh
west init -m https://github.com/nxp-mcuxpresso/mcuxsdk-manifests.git \
  mcuxpresso-sdk --mr v26.09.00
cd mcuxpresso-sdk
west update
```

The full SDK checkout downloads multiple repositories and requires several gigabytes of disk space. NXP also documents a targeted board repository update workflow; use the manifest's board selection for `evkmimxrt1180` if you want to reduce the checkout.

Verify the selected manifest revision:

```sh
west manifest --resolve
west list
```

## Build the upstream dual-core reference example

From the MCUXpresso SDK workspace root:

```sh
west build -b evkmimxrt1180 --sysbuild \
  examples/multicore_examples/hello_world/primary \
  --config flexspi_nor_debug --toolchain armgcc \
  -Dcore_id=cm33 -p always
```

The repository's CI workflow first builds this unmodified NXP reference example. It then creates a temporary copy of the NXP `hello_world` SysBuild project, replaces only the two application `main.c` files with the sources from `examples/dual_core_led_toggle`, and builds the resulting CM33 + CM7 LED-toggle firmware. NXP board support, linker scripts, startup, image-copy and packaging configuration remain sourced from the pinned SDK.

## Build validation

The GitHub Actions workflow `RT1189 embedded build` validates two distinct outputs:

1. The unmodified NXP CM33 + CM7 multicore baseline.
2. This repository's CM33 + CM7 dual-core LED-toggle application, built through the NXP SysBuild project template.

The workflow uploads generated ELF, BIN, HEX and MAP files as a build artifact. A successful CI build confirms compilation and image packaging; it does not replace a physical EVK test of the LED, inter-core timing or boot behaviour.

NXP documentation:
- SDK 26.09.00 release: https://github.com/nxp-mcuxpresso/mcuxsdk-manifests/releases/tag/v26.09.00
- MIMXRT1180-EVK board page: https://docs.mcuxpresso.nxp.com/mcuxsdk/26.09.00/boards/RT/evkmimxrt1180/index.html
- Repository setup: https://docs.mcuxpresso.nxp.com/mcuxsdk/26.09.00/gsd/repo_setup.html
- SysBuild: https://docs.mcuxpresso.nxp.com/mcuxsdk/26.09.00/develop/build_system/Sysbuild.html
