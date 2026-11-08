
# --- AIED MCUboot application variant (appended by scripts/build_rt1180.*) ---
# Links the CM33 primary image, which embeds the CM7 image, for the MCUboot primary slot
# of the NXP reference layout (ADR-0014). Mirrors NXP ota_mcuboot_basic for evkmimxrt1180.

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}
    INCLUDES ${board_root}/${board}/ota_examples/mcuboot_opensource
             ${board_root}/${board}/ota_examples/mcuboot_opensource/flash_partitioning
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}
    SOURCES ${board_root}/${board}/ota_examples/mcuboot_opensource/flash_partitioning/flash_partitioning.c
)

include(${SdkRootDirPath}/middleware/mcuboot_opensource/CMakeLists.txt)

mcux_remove_armgcc_linker_script(
    BASE_PATH ${SdkRootDirPath}
    TARGETS flexspi_nor_debug flexspi_nor_release
    LINKER ${device_root}/RT/RT1180/MIMXRT1189/gcc/MIMXRT1189xxxxx_cm33_flexspi_nor.ld
)

mcux_add_armgcc_linker_script(
    BASE_PATH ${SdkRootDirPath}
    TARGETS flexspi_nor_debug flexspi_nor_release
    LINKER ${board_root}/${board}/ota_examples/ota_linker/MIMXRT1189xxxxx_cm33_flexspi_nor_mcuboot.ld
)

# The Boot ROM starts MCUboot, not the application: no FCB/XMCD/container in this image.
mcux_remove_configuration(
    TARGETS flexspi_nor_debug flexspi_nor_release
    CC "-DXIP_BOOT_HEADER_ENABLE=1"
)

mcux_add_configuration(
    TARGETS flexspi_nor_debug flexspi_nor_release
    CC "-DXIP_BOOT_HEADER_ENABLE=0"
)

mcux_add_macro(
    CC "-DAIED_MCUBOOT_APP=1"
)

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)
