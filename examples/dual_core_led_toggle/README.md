# Dual-core LED toggle (MIMXRT1180-EVK / MIMXRT1189)

## Behaviour

- CM33 is the primary core.
- CM33 starts CM7 and waits for the CM7-ready event.
- Once per second CM33 sends `APP_EVENT_TOGGLE_LED` using NXP MCMGR.
- CM7 receives the event, records a pending request, and toggles the EVK green user LED from its main loop.
- The LED is connected to GPIO_AD_27 / GPIO4_IO27 on the MIMXRT1180-EVK.

The application uses no RTOS. MCMGR is the inter-core event mechanism supplied by the MCUXpresso Multicore SDK.

## SDK baseline

Use an MCUXpresso SDK package for the **MIMXRT1180-EVK / MIMXRT1189** that contains the multicore examples and MCMGR. NXP's 26.09.00 documentation lists this board as supported for multicore examples and documents the `hello_world` and `multicore_manager` projects:

- https://docs.mcuxpresso.nxp.com/mcuxsdk/26.09.00-pvw1/html/examples/multicore_examples/hello_world/readme.html
- https://docs.mcuxpresso.nxp.com/mcuxsdk/latest/middleware/multicore/mcmgr/index.html

The SDK is an external dependency and is deliberately not vendored into this repository.

## Integration with the NXP board projects

Start from the SDK's **MIMXRT1180-EVK multicore `hello_world`** example. Keep the SDK-provided project/build metadata, startup files, clock and pin configuration, linker scripts, CM7 image packaging and multicore image-generation steps. Replace the application source files with the corresponding files in this directory:

| SDK project | Application source |
|---|---|
| `hello_world_cm33` | `cm33/main.c` |
| `hello_world_cm7` | `cm7/main.c` |

Add `common/` to both projects' include paths and add the SDK MCMGR component to both link targets. Keep all SDK-generated board and startup sources.

### CM7 image address

The CM33 project must link/package the CM7 image as the SDK multicore example does. Define `APP_CM7_BOOT_ADDRESS` for the CM33 compilation using the CM7 image's actual load/entry address from the selected SDK linker/image configuration. Do not copy an address from another board, memory target, or SDK release. The source intentionally fails compilation if this board-specific value is missing.

### Build order

For the classic Arm GCC projects, NXP's board guide specifies building the auxiliary CM7 image first, then the primary CM33 image because the CM33 link/package step consumes the CM7 output. Use the SDK-provided build scripts for the selected target (RAM or FlexSPI NOR). Do not use a generic single-core linker script.

For an SDK repository delivery, use the SDK's documented `west`/sysbuild flow for the matching board and example, then integrate these application sources into the linked multicore projects. The exact board target and image configuration must match the installed SDK release.

## Important implementation notes

- `MCMGR_EarlyInit()` and `MCMGR_Init()` are called on both cores.
- CM33 registers its event handler before releasing CM7.
- CM7 registers its handler before sending `APP_EVENT_CM7_READY`.
- The MCMGR callback only sets a flag. GPIO access remains in the CM7 main loop.
- The LED GPIO mux and GPIO4 clock are configured by the CM7 application. Confirm the pin symbol and polarity against the exact board revision and SDK package before hardware testing.
- This example is a source-level integration on top of NXP's generated multicore project. It is not a standalone CMake target and has not yet been compiled or run on physical hardware in this repository environment.

## NXP references

- MIMXRT1180-EVK getting started guide: https://mcuxpresso.nxp.com/mcuxsdk/latest/boards/RT/evkmimxrt1180/gettingStarted/topics/ide_run_a_multicore_example_application.html
- MCMGR API and event model: https://mcuxpresso.nxp.com/mcuxsdk/latest/middleware/multicore/mcmgr/index.html
- MIMXRT1180-EVK pin usage (GPIO_AD_27 LED): https://docs.zephyrproject.org/latest/boards/nxp/mimxrt1180_evk/doc/index.html
