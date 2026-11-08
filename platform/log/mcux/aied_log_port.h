#ifndef AIED_LOG_PORT_H
#define AIED_LOG_PORT_H

/**
 * @file aied_log_port.h
 * @brief MCUXpresso SDK port of the logging service (ADR-0015): time source, console,
 *        persistent littlefs log region and the UART `logdump` command.
 *
 * Requires BOARD_InitHardware() (debug console) before aied_log_port_init().
 * Uses SysTick (SysTick_Handler) and the DWT cycle counter.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Size at which a new log file is started. */
#ifndef AIED_LOG_PORT_FILE_SIZE
#define AIED_LOG_PORT_FILE_SIZE (16U * 1024U)
#endif

/** @brief Number of log files kept in the log region. */
#ifndef AIED_LOG_PORT_FILES
#define AIED_LOG_PORT_FILES 4U
#endif

/**
 * @brief Start the time source, the console backend and, if the flash log region can be
 *        mounted, the persistent file backend.
 * @param firmware_version Version string written in the boot record.
 * @return true when persistent logging is active; console logging works in any case.
 */
bool aied_log_port_init(const char *firmware_version);

/**
 * @brief Write buffered records to flash and serve the UART `logdump` command.
 *        Call often from the main loop (never from an interrupt).
 */
void aied_log_port_poll(void);

/**
 * @brief Microseconds since aied_log_port_init(), 64-bit, unaffected by masked interrupts
 *        shorter than one CYCCNT wrap (about 17 s at 240 MHz).
 * @return Uptime in microseconds.
 */
uint64_t aied_log_port_now_us(void);

#ifdef __cplusplus
}
#endif

#endif
