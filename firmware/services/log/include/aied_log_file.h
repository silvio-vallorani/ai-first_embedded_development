#ifndef AIED_LOG_FILE_H
#define AIED_LOG_FILE_H

/**
 * @file aied_log_file.h
 * @brief Persistent log backend: rotating text files on a file system (ADR-0015).
 *
 * Records are appended to `log.NNNNNNNN.txt` files. When the newest file would exceed the
 * size limit a new file is started, and the oldest files are removed so that at most
 * `max_files` remain. A boot counter is kept in `boot.cnt`.
 */

#include "aied_log.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief RAM batch written to the file system per flush. */
#ifndef AIED_LOG_FILE_BUFFER_SIZE
#define AIED_LOG_FILE_BUFFER_SIZE 512U
#endif

/** @brief Maximum file name length including NUL ("log.NNNNNNNN.txt"). */
#define AIED_LOG_FILE_NAME_MAX 24U

/** @brief Callback receiving one file name during aied_log_fs_t.list. */
typedef void (*aied_log_fs_name_fn)(void *callback_context, const char *name);

/**
 * @brief File system operations used by the backend (root directory only).
 *
 * All functions return 0 on success and a negative value on failure.
 */
typedef struct {
    int (*list)(void *context, aied_log_fs_name_fn callback, void *callback_context);
    int (*size)(void *context, const char *name, uint32_t *size);
    /** Create the file if needed and append; data is durable when the call returns. */
    int (*append)(void *context, const char *name, const void *data, size_t length);
    int (*read)(void *context, const char *name, uint32_t offset, void *data, size_t length, size_t *read);
    int (*remove)(void *context, const char *name);
    /** Replace the whole file content. */
    int (*write_file)(void *context, const char *name, const void *data, size_t length);
    void *context;
} aied_log_fs_t;

/** @brief Receiver for aied_log_file_dump(). */
typedef struct {
    void (*file_begin)(void *context, const char *name, uint32_t size);
    void (*data)(void *context, const char *data, size_t length);
    void (*file_end)(void *context, const char *name);
    void *context;
} aied_log_dump_sink_t;

/** @brief Backend state; treat as opaque. */
typedef struct {
    const aied_log_fs_t *fs;
    uint32_t max_file_size;
    uint32_t max_files;
    uint32_t first_index;
    uint32_t last_index;
    bool has_files;
    uint32_t current_size;
    uint32_t boot_count;
    uint32_t write_errors;
    size_t buffered;
    char buffer[AIED_LOG_FILE_BUFFER_SIZE];
    aied_log_backend_t backend;
} aied_log_file_t;

/**
 * @brief Scan existing log files, advance the boot counter and prepare the backend.
 * @param file Backend state.
 * @param fs Mounted file system; must stay valid.
 * @param max_file_size Size at which a new file is started (bytes, at least one line).
 * @param max_files Number of files kept (at least 1).
 * @return true on success; the backend is usable even if the boot counter could not be stored.
 */
bool aied_log_file_init(aied_log_file_t *file, const aied_log_fs_t *fs, uint32_t max_file_size, uint32_t max_files);

/**
 * @brief Deferred backend descriptor to pass to aied_log_backend_add().
 * @param file Initialised backend state.
 * @return Backend descriptor.
 */
const aied_log_backend_t *aied_log_file_backend(aied_log_file_t *file);

/**
 * @brief Boot number of this run (1 on the first boot after the file system is created).
 * @param file Initialised backend state.
 * @return Boot number.
 */
uint32_t aied_log_file_boot_count(const aied_log_file_t *file);

/**
 * @brief Number of batches that could not be written (lost).
 * @param file Initialised backend state.
 * @return Write error count.
 */
uint32_t aied_log_file_write_errors(const aied_log_file_t *file);

/**
 * @brief Write buffered records, then stream all log files, oldest first.
 * @param file Initialised backend state.
 * @param sink Receiver.
 * @return true when every file was read completely.
 */
bool aied_log_file_dump(aied_log_file_t *file, const aied_log_dump_sink_t *sink);

#ifdef __cplusplus
}
#endif

#endif
