#ifndef AIED_LOG_H
#define AIED_LOG_H

/**
 * @file aied_log.h
 * @brief Zephyr-style logging service (ADR-0015).
 *
 * Usage:
 * @code
 * #include "aied_log.h"
 * LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);
 * LOG_INF("started, version %s", version);
 * @endcode
 *
 * Every record is exactly one text line: `[hh:mm:ss.mmm,uuu] <lvl> module: message\n`.
 * Line breaks inside the message are replaced by spaces; a trailing line break is dropped.
 * A message longer than the line buffer is truncated and ends with "...".
 *
 * Immediate backends (console) are called in the caller's context. Deferred backends
 * (persistent file) receive records from a RAM ring buffer when aied_log_process() runs
 * in the main loop, never from the logging call itself.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(LOG_ERR) || defined(LOG_WRN) || defined(LOG_INF) || defined(LOG_DBG)
#error "LOG_* macros already defined (NXP component.log?); do not mix it with aied_log.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** @name Log levels (same values as Zephyr). */
/** @{ */
#define LOG_LEVEL_NONE 0U
#define LOG_LEVEL_ERR  1U
#define LOG_LEVEL_WRN  2U
#define LOG_LEVEL_INF  3U
#define LOG_LEVEL_DBG  4U
/** @} */

/** @brief Compile-time maximum level; calls above it are removed by the compiler. */
#ifndef AIED_LOG_MAX_LEVEL
#define AIED_LOG_MAX_LEVEL LOG_LEVEL_DBG
#endif

/** @brief Maximum length of one formatted line, including the terminating '\n'. */
#ifndef AIED_LOG_LINE_MAX
#define AIED_LOG_LINE_MAX 160U
#endif

/** @brief Size of the RAM ring buffer that holds records for deferred backends. */
#ifndef AIED_LOG_BUFFER_SIZE
#define AIED_LOG_BUFFER_SIZE 2048U
#endif

/** @brief Maximum number of registered backends. */
#define AIED_LOG_MAX_BACKENDS 4U

/** @brief A log source; created by LOG_MODULE_REGISTER(). */
typedef struct {
    const char *name; /**< Module name printed in every record. */
    uint32_t level;   /**< Most verbose level enabled for this module. */
} aied_log_module_t;

/** @brief Output backend. */
typedef struct {
    /** Output one complete line (ends with '\n', not NUL-terminated). */
    void (*write)(void *context, const char *line, size_t length);
    /** Optional: called at the end of aied_log_process() for deferred backends. */
    void (*flush)(void *context);
    void *context;
    bool deferred; /**< true: buffered and written by aied_log_process(). */
} aied_log_backend_t;

/** @brief Service configuration. */
typedef struct {
    uint64_t (*now_us)(void); /**< Monotonic time since boot in microseconds. */
    /** Optional critical section around the ring buffer (ISR safety). */
    uint32_t (*lock)(void);
    void (*unlock)(uint32_t key);
} aied_log_config_t;

/**
 * @brief Initialise the service and drop all buffered records and backends.
 * @param config Time source and optional lock; must stay valid. now_us is required.
 * @return true on success.
 */
bool aied_log_init(const aied_log_config_t *config);

/**
 * @brief Register an output backend.
 * @param backend Backend descriptor; must stay valid.
 * @return false when the table is full or the backend is invalid.
 */
bool aied_log_backend_add(const aied_log_backend_t *backend);

/**
 * @brief Format a record and dispatch it. Use the LOG_* macros instead.
 * @param module Source module.
 * @param level Record level.
 * @param format printf-style format.
 */
void aied_log_write(const aied_log_module_t *module, uint32_t level, const char *format, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;

/**
 * @brief Move buffered records to deferred backends; call from the main loop.
 * @return Number of records written (including a "messages dropped" record).
 */
size_t aied_log_process(void);

/**
 * @brief Number of records lost because the ring buffer was full, since the last report.
 * @return Pending dropped-record count.
 */
uint32_t aied_log_dropped(void);

/**
 * @brief Format one record into a buffer; exposed for tests and backends.
 * @param buffer Destination, at least AIED_LOG_LINE_MAX bytes.
 * @param timestamp_us Record time.
 * @param module_name Module name.
 * @param level Record level.
 * @param message Message text (may contain line breaks).
 * @return Line length including the terminating '\n'.
 */
size_t aied_log_format_line(char *buffer, uint64_t timestamp_us, const char *module_name, uint32_t level,
                            const char *message);

/** @brief Register a log module in the current translation unit (Zephyr API). */
#define LOG_MODULE_REGISTER(module_name, module_level)                                                                 \
    static const aied_log_module_t aied_log_module_ = {#module_name, (module_level)}

#define AIED_LOG_(lvl, ...)                                                                                            \
    do {                                                                                                               \
        if (((lvl) <= AIED_LOG_MAX_LEVEL) && ((lvl) <= aied_log_module_.level)) {                                      \
            aied_log_write(&aied_log_module_, (lvl), __VA_ARGS__);                                                     \
        }                                                                                                              \
    } while (0)

#define LOG_ERR(...) AIED_LOG_(LOG_LEVEL_ERR, __VA_ARGS__)
#define LOG_WRN(...) AIED_LOG_(LOG_LEVEL_WRN, __VA_ARGS__)
#define LOG_INF(...) AIED_LOG_(LOG_LEVEL_INF, __VA_ARGS__)
#define LOG_DBG(...) AIED_LOG_(LOG_LEVEL_DBG, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif
