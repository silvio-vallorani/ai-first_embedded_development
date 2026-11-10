
# --- AIED logging service (appended by scripts/build_rt1180.*; AIED_ROOT set by the script) ---
# Zephyr-style log core, rotating text files on littlefs in the log region (ADR-0015).

mcux_add_source(
    BASE_PATH ${AIED_ROOT}
    SOURCES firmware/services/log/src/aied_log.c
            firmware/services/log/src/aied_log_file.c
            platform/storage/littlefs/aied_log_fs_littlefs.c
            platform/log/mcux/aied_log_port.c
)

mcux_add_include(
    BASE_PATH ${AIED_ROOT}
    INCLUDES firmware/services/log/include
             platform/storage/littlefs
             platform/log/mcux
)

# littlefs and the log formatter overflow the SDK linker default stack (1 KiB, MSPLIM HardFault seen on
# hardware, 2026-11-09); 8 KiB as in the SDK flash examples.
mcux_add_armgcc_configuration(
    LD "-Xlinker --defsym=__stack_size__=0x2000"
)
