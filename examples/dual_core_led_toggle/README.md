# Dual-core LED toggle (MIMXRT1180-EVK / MIMXRT1189)

## Behaviour

- CM33 is the primary core.
- CM33 starts CM7 and waits for the CM7-ready event.
- Once per second CM33 sends `APP_EVENT_TOGGLE_LED` using NXP MCMGR.
- CM7 receives the event, records a pending request, and toggles the EVK green user LED from its main loop.
- The LED is connected to GPIO_AD_27 / GPIO4_IO27 on the MIMXRT1180-EVK.

The application uses no RTOS. MCMGR is the inter-core event mechanism supplied by the MCUXpresso Multicore SDK.

## Selected SDK and toolchain

- MCUXpresso SDK: **26.09.00**, manifest tag `v26.09.00`.
- Board: `evkmimxrt1180` (MIMXRT1189).
- Build system: West + CMake + Ninja + SysBuild.
- Toolchain: Arm GNU Toolchain **14.2.x**.

See [MCUXpresso SDK setup](../../docs/toolchains/MCUXPRESSO_SDK.md) for workspace initialization and build prerequisites.

## Integration with the NXP board projects

Start from the SDK's **MIMXRT1180-EVK multicore `hello_world`** SysBuild example. Keep the SDK-provided project/build metadata, startup files, clock and pin configuration, linker scripts, CM7 image packaging and multicore image-generation steps. Replace the application source files with the corresponding files in this directory:

| SDK project | Application source |
|---|---|
| Primary CM33 project | `cm33/main.c` |
| Secondary CM7 project | `cm7/main.c` |

Add `common/` to both projects' include paths and add the SDK MCMGR component to both link targets. Keep all SDK-generated board and startup sources.

### CM7 image address

The CM33 project must link/package the CM7 image as the SDK multicore example does. Define `APP_CM7_BOOT_ADDRESS` for the CM33 compilation using the CM7 image's actual load/entry address from the selected SDK linker/image configuration. Do not copy an address from another board, memory target, or SDK release. The source intentionally fails compilation if this board-specific value is missing.

### Build order and configuration

Use the SDK-provided SysBuild project so the CM7 image is built and packaged as a dependency of the primary CM33 image. For the reference FlexSPI NOR debug configuration, the upstream baseline command is:

```sh
west build -b evkmimxrt1180 --sysbuild \
  examples/multicore_examples/hello_world/primary \
  --config flexspi_nor_debug --toolchain armgcc \
  -Dcore_id=cm33 -p always
```

This command builds the unmodified NXP reference example. After integrating the application sources and board-specific configuration, use the same SysBuild entry point and configuration to build this application. Do not use a generic single-core linker script.

## Important implementation notes

- `MCMGR_EarlyInit()` and `MCMGR_Init()` are called on both cores.
- CM33 registers its event handler before releasing CM7.
- CM7 registers its handler before sending `APP_EVENT_CM7_READY`.
- The MCMGR callback only sets a flag. GPIO access remains in the CM7 main loop.
- The LED GPIO mux and GPIO4 clock are configured by the CM7 application. Confirm the pin symbol and polarity against the exact board revision and SDK package before hardware testing.
- This is application source integrated on top of NXP's generated multicore project. It is not yet a standalone CMake target and has not been compiled or run on physical hardware in this repository environment.

## NXP references

- [MCUXpresso SDK 26.09.00 – MIMXRT1180-EVK](https://docs.mcuxpresso.nxp.com/mcuxsdk/26.09.00/boards/RT/evkmimxrt1180/index.html)
- [MCUXpresso SDK 26.09.00 – multicore_manager example](https://docs.mcuxpresso.nxp.com/mcuxsdk/26.09.00/examples/multicore_examples/multicore_manager/readme.html)
- [MCUXpresso SDK – MCMGR documentation](https://docs.mcuxpresso.nxp.com/mcuxsdk/26.09.00/middleware/multicore/mcmgr/index.html)
- [MCUXpresso SDK 26.09.00 release](https://github.com/nxp-mcuxpresso/mcuxsdk-manifests/releases/tag/v26.09.00)
