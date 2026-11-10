# Dual-core LED toggle (AM254-4 / MIMXRT1180-EVK, MIMXRT1189)

## Behaviour

- CM33 is the primary core.
- CM33 starts CM7 and waits for the CM7-ready event.
- Once per second CM33 sends `APP_EVENT_TOGGLE_LED` using NXP MCMGR.
- CM7 receives the event, records a pending request, and toggles the board user LED from its main loop.
- The LED comes from `boards/<board>/include/aied_board.h`: DL1 (green, GPIO_EMC_B1_41 / GPIO3_IO09) on the AM254-4, the green user LED (GPIO_AD_27 / GPIO4_IO27) on the MIMXRT1180-EVK. Select the board with `--board am254-4` (`-Board am254-4`); the default is the EVK.

The application uses no RTOS. MCMGR is the inter-core event mechanism supplied by the MCUXpresso Multicore SDK.

## Selected SDK and toolchain

- MCUXpresso SDK: **26.09.00**, manifest tag `v26.09.00`.
- Board: `evkmimxrt1180` (MIMXRT1189).
- Build system: West + CMake + Ninja + SysBuild.
- Toolchain: Arm GNU Toolchain **14.2.x**.

See [MCUXpresso SDK setup](../../docs/toolchains/MCUXPRESSO_SDK.md) for workspace initialization and build prerequisites.

## Build integration

The CI workflow builds both the unmodified NXP multicore baseline and this application using the same pinned SDK workspace and NXP SysBuild project metadata.

For the application build, `scripts/build_rt1180.sh` (CI and Linux/macOS) or `scripts/build_rt1180.ps1` (Windows):

1. Copies the NXP `multicore_examples/hello_world` project directory to a generated `aied_dual_core_led_toggle` sibling inside the SDK examples tree, marked with `.aied-generated-overlay`.
2. Replaces the primary and secondary `main.c` application sources with this repository's CM33 and CM7 sources.
3. Copies the shared `app_events.h` into both project directories.
4. Renames the template image names to `led_toggle_primary_core` and `led_toggle_secondary_core`, consistently in the CMake, SysBuild and IDE metadata files.
5. Retains NXP's board port (`multicore_examples/hello_world` board path), `prj.conf`, board-specific `app.h`, clock/pin setup, linker scripts, CM7 image packaging and generated startup sources.
6. Builds the copied project with the same SysBuild configuration used for the upstream baseline, into `build/rt1180-led-toggle`.

The resulting images are `led_toggle_primary_core_cm33.elf` (CM33) and `led_toggle_secondary_core_cm7.elf` (CM7); CI fails if they are missing or if a template image name remains. This keeps the SDK project scaffolding and board support intact while compiling the application sources maintained in this repository. The generated overlay is not committed.

### MCUboot variant (ADR-0014)

`build_rt1180.sh --mcuboot` (or `build_rt1180.ps1 -McuBoot`) builds the same application for the MCUboot primary slot, into `build/rt1180-led-mcuboot`. It appends `mcuboot/primary.cmake` and `mcuboot/primary.conf` to the generated primary project:

- NXP's MCUboot application linker (`ota_linker/MIMXRT1189xxxxx_cm33_flexspi_nor_mcuboot.ld`): the vector table is at 0x28040400, after the 1-KiB MCUboot header, and the CM7 image stays embedded in the CM33 image.
- No FCB, XMCD or boot container (`XIP_BOOT_HEADER_ENABLE=0`): the Boot ROM starts MCUboot, not the application.
- MCUboot application support and the `mflash` driver. After CM7 reports ready, CM33 marks a test image as permanent.

Options for update tests: `--version X.Y.Z` (printed at start-up), `--toggle-period-us N`, and `--no-confirm` (never confirm a test image, to test rollback). `scripts/rt1180_mcuboot.py` builds, signs and flashes the complete update-test set; see `docs/engineering/MCUBOOT_UPDATE_TEST.md`.

At start-up CM33 logs the boot number, the version, the toggle period, `CM7 ready` and the MCUboot image state. Logs go to the debug UART (115200 baud) and to rotating text files in flash; see `docs/engineering/LOGGING.md`.

### Logging (both variants)

The build scripts always append `log/primary.cmake` and `log/primary.conf`. These add the logging service (`firmware/services/log`), its MCUXpresso port (`platform/log/mcux`) and the littlefs file-system adapter (`platform/storage/littlefs`), and enable littlefs + `mflash` on the log region at QSPI offset 0x7E0000 (ADR-0015). Standalone builds (without `--mcuboot`) also append `standalone/primary.cmake`, which links with NXP's bootloader XIP linker so that `mflash` and the FlexSPI driver run from ITCM while they reprogram the NOR (ADR-0015). The CM33 main loop is non-blocking: it toggles the LED on time and calls `aied_log_port_poll()` continuously, which writes buffered records and answers the UART `logdump` command.

### CM7 image address and packaging

The CM33 application includes the NXP board-specific `app.h`. For the MIMXRT1180-EVK SDK configuration, this supplies `CORE1_BOOT_ADDRESS` and the secondary-image linker symbols. The application uses those SDK definitions and, when `CORE1_IMAGE_COPY_TO_RAM` is enabled, performs the same secondary-image copy step as the NXP reference application.

Do not replace these values with addresses copied from another board, memory target, or SDK release.

## Important implementation notes

- CM33 initializes MCMGR and board hardware, registers its event handler, then starts CM7.
- CM7 initializes board hardware and the LED GPIO, initializes MCMGR, registers its event handler, and then sends `APP_EVENT_CM7_READY`.
- The MCMGR callback only sets a flag. GPIO access remains in the CM7 main loop.
- The LED pad is muxed by the board's `BOARD_InitPins()` (SDK pin mux on the EVK, `boards/am254-4/sdk/cm7/pin_mux.c` on the AM254-4); the CM7 application then configures the GPIO.
- CI compilation validates both core images and SysBuild packaging. Physical LED behaviour still requires a test on the board.

## NXP references

- [MCUXpresso SDK 26.09.00 – MIMXRT1180-EVK](https://docs.mcuxpresso.nxp.com/mcuxsdk/26.09.00/boards/RT/evkmimxrt1180/index.html)
- [MCUXpresso SDK 26.09.00 – multicore hello_world](https://github.com/nxp-mcuxpresso/mcuxsdk-examples/tree/release/26.09.00/multicore_examples/hello_world)
- [MCUXpresso SDK 26.09.00 release](https://github.com/nxp-mcuxpresso/mcuxsdk-manifests/releases/tag/v26.09.00)
