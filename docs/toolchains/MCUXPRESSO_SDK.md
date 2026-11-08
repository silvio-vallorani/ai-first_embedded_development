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

NXP's release page identifies 26.09.00 as the latest non-preview quarterly release at the time this baseline was selected. NXP also offers 26.06.00-LTS for projects prioritizing the two-year maintenance window. The 26.12.00-pvw1 preview is intentionally not selected.

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

This is the upstream NXP reference build, not yet the custom LED-toggle application. It is the first baseline to build successfully before changing application sources. SysBuild is required so the secondary-core image is built and packaged with the primary image.

For the SDK's MCMGR-specific demonstration, inspect:

```text
examples/multicore_examples/multicore_manager/
```

NXP documentation:
- SDK 26.09.00 release: https://github.com/nxp-mcuxpresso/mcuxsdk-manifests/releases/tag/v26.09.00
- MIMXRT1180-EVK board page: https://docs.mcuxpresso.nxp.com/mcuxsdk/26.09.00/boards/RT/evkmimxrt1180/index.html
- Multicore SDK release notes: https://docs.mcuxpresso.nxp.com/mcuxsdk/26.09.00/middleware/multicore/mcsdk-release-notes.html
- Repository setup: https://docs.mcuxpresso.nxp.com/mcuxsdk/26.09.00/gsd/repo_setup.html
- SysBuild: https://docs.mcuxpresso.nxp.com/mcuxsdk/26.09.00/develop/build_system/Sysbuild.html

## Validation status

The version selection and build procedure are documented, but the SDK has not been downloaded or built in this execution environment. The upstream reference build and then the modified CM33/CM7 application must be compiled before claiming target-build success. Physical LED behaviour still requires an EVK test.
