#include "aied_log_port.h"

#include "aied_log.h"
#include "aied_log_file.h"
#include "aied_log_fs_littlefs.h"
#include "board.h"
#include "fsl_common.h"
#include "fsl_debug_console.h"
#include "fsl_lpuart.h"
#include "lfs_mflash.h"

#include <string.h>

LOG_MODULE_REGISTER(log, LOG_LEVEL_INF);

#define LOGDUMP_COMMAND "logdump"
#define COMMAND_MAX     16U

static uint64_t s_cycles;
static uint32_t s_last_cycles;
static uint32_t s_cycles_per_us;

static aied_log_fs_littlefs_t s_littlefs;
static aied_log_fs_t s_fs;
static aied_log_file_t s_file;
static bool s_persistent;

static char s_command[COMMAND_MAX];
static size_t s_command_length;

static uint32_t irq_lock(void)
{
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

static void irq_unlock(uint32_t primask)
{
    __set_PRIMASK(primask);
}

/* Extend the 32-bit cycle counter; must run at least once per wrap (SysTick does). */
static uint64_t cycles_now(void)
{
    const uint32_t key = irq_lock();
    const uint32_t now = MSDK_GetCpuCycleCount();
    uint64_t value;

    s_cycles += (uint32_t)(now - s_last_cycles);
    s_last_cycles = now;
    value = s_cycles;
    irq_unlock(key);
    return value;
}

/**
 * @brief SysTick interrupt: keeps the 64-bit cycle extension ahead of CYCCNT wraps.
 */
void SysTick_Handler(void)
{
    (void)cycles_now();
}

uint64_t aied_log_port_now_us(void)
{
    return (s_cycles_per_us == 0U) ? 0U : (cycles_now() / s_cycles_per_us);
}

/* Terminals expect CRLF; files keep LF. */
static void console_put(const char *text, size_t length)
{
    for (size_t i = 0U; i < length; i++) {
        if (text[i] == '\n') {
            (void)PUTCHAR('\r');
        }
        (void)PUTCHAR(text[i]);
    }
}

static void console_write(void *context, const char *line, size_t length)
{
    (void)context;
    console_put(line, length);
}

static void dump_begin(void *context, const char *name, uint32_t size)
{
    (void)context;
    (void)PRINTF("=== FILE %s %u ===\r\n", name, (unsigned)size);
}

static void dump_data(void *context, const char *data, size_t length)
{
    (void)context;
    console_put(data, length);
}

static void dump_end(void *context, const char *name)
{
    (void)context;
    (void)name;
    (void)PRINTF("=== END FILE ===\r\n");
}

static void dump_logs(void)
{
    static const aied_log_dump_sink_t sink = {dump_begin, dump_data, dump_end, NULL};

    if (!s_persistent) {
        (void)PRINTF("=== AIED LOG DUMP UNAVAILABLE ===\r\n");
        return;
    }
    (void)aied_log_process();
    (void)PRINTF("=== AIED LOG DUMP BEGIN boot %u ===\r\n", (unsigned)aied_log_file_boot_count(&s_file));
    (void)PRINTF("=== AIED LOG DUMP %s ===\r\n", aied_log_file_dump(&s_file, &sink) ? "END" : "INCOMPLETE");
}

static void poll_command(void)
{
    LPUART_Type *uart = (LPUART_Type *)BOARD_DEBUG_UART_BASEADDR;

    if ((LPUART_GetStatusFlags(uart) & (uint32_t)kLPUART_RxOverrunFlag) != 0U) {
        (void)LPUART_ClearStatusFlags(uart, (uint32_t)kLPUART_RxOverrunFlag);
        s_command_length = COMMAND_MAX; /* discard the damaged line */
    }
    while ((LPUART_GetStatusFlags(uart) & (uint32_t)kLPUART_RxDataRegFullFlag) != 0U) {
        const char c = (char)LPUART_ReadByte(uart);

        if ((c == '\r') || (c == '\n')) {
            if ((s_command_length == (sizeof(LOGDUMP_COMMAND) - 1U)) &&
                (memcmp(s_command, LOGDUMP_COMMAND, s_command_length) == 0)) {
                dump_logs();
            }
            s_command_length = 0U;
        } else if (s_command_length < COMMAND_MAX) {
            s_command[s_command_length++] = c;
        }
    }
}

bool aied_log_port_init(const char *firmware_version)
{
    static const aied_log_config_t config = {aied_log_port_now_us, irq_lock, irq_unlock};
    static const aied_log_backend_t console = {console_write, NULL, NULL, false};
    struct lfs_config base;

    MSDK_EnableCpuCycleCounter();
    s_last_cycles = MSDK_GetCpuCycleCount();
    s_cycles = 0U;
    s_cycles_per_us = SystemCoreClock / 1000000U;
    (void)SysTick_Config(SystemCoreClock / 100U); /* 10 ms, far below one CYCCNT wrap */

    (void)aied_log_init(&config);
    (void)aied_log_backend_add(&console);

    s_persistent = false;
    if ((lfs_get_default_config(&base) == 0) && (lfs_storage_init(&base) == 0) &&
        (aied_log_fs_littlefs_mount(&s_littlefs, &base) == 0)) {
        s_fs = aied_log_fs_littlefs_interface(&s_littlefs);
        s_persistent = aied_log_file_init(&s_file, &s_fs, AIED_LOG_PORT_FILE_SIZE, AIED_LOG_PORT_FILES) &&
                       aied_log_backend_add(aied_log_file_backend(&s_file));
    }

    if (s_persistent) {
        LOG_INF("boot %u, firmware %s%s", (unsigned)aied_log_file_boot_count(&s_file), firmware_version,
                s_littlefs.formatted ? ", log storage formatted" : "");
    } else {
        LOG_ERR("firmware %s, persistent log storage unavailable", firmware_version);
    }
    return s_persistent;
}

void aied_log_port_poll(void)
{
    (void)aied_log_process();
    poll_command();
}
