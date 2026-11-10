
# --- AIED standalone (direct ROM boot) variant (appended by scripts/build_rt1180.*) ---
# The log service programs the NOR it executes from (mflash), so the FlexSPI and mflash drivers
# must run from ITCM, as NXP does for MCUboot. NXP's bootloader XIP linker is the standard CM33
# flexspi_nor linker plus exactly that relocation (ADR-0015).

mcux_remove_armgcc_linker_script(
    BASE_PATH ${SdkRootDirPath}
    TARGETS flexspi_nor_debug flexspi_nor_release
    LINKER ${device_root}/RT/RT1180/MIMXRT1189/gcc/MIMXRT1189xxxxx_cm33_flexspi_nor.ld
)

mcux_add_armgcc_linker_script(
    BASE_PATH ${SdkRootDirPath}
    TARGETS flexspi_nor_debug flexspi_nor_release
    LINKER ${board_root}/${board}/ota_examples/mcuboot_opensource/linker/MIMXRT1189xxxxx_cm33_flexspi_nor.ld
)
