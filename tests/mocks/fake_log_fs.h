#ifndef AIED_FAKE_LOG_FS_H
#define AIED_FAKE_LOG_FS_H

#include "aied_log_file.h"

#define FAKE_LOG_FS_MAX_FILES 12U
#define FAKE_LOG_FS_FILE_SIZE 4096U

/** @brief One file of the in-memory file system. */
typedef struct {
    bool used;
    char name[AIED_LOG_FILE_NAME_MAX];
    char data[FAKE_LOG_FS_FILE_SIZE];
    uint32_t size;
} fake_log_file_t;

/** @brief In-memory root directory with failure injection. */
typedef struct {
    fake_log_file_t files[FAKE_LOG_FS_MAX_FILES];
    bool fail_append;
    bool fail_list;
    uint32_t append_calls;
} fake_log_fs_t;

/**
 * @brief Reset the fake to an empty file system.
 * @param fake Fake instance.
 */
void fake_log_fs_init(fake_log_fs_t *fake);

/**
 * @brief Build the file system interface bound to the fake.
 * @param fake Fake instance.
 * @return Interface.
 */
aied_log_fs_t fake_log_fs_interface(fake_log_fs_t *fake);

/**
 * @brief Find a file by name.
 * @param fake Fake instance.
 * @param name File name.
 * @return The file, or NULL.
 */
fake_log_file_t *fake_log_fs_find(fake_log_fs_t *fake, const char *name);

/**
 * @brief Number of files whose name starts with "log.".
 * @param fake Fake instance.
 * @return Log file count.
 */
uint32_t fake_log_fs_log_files(const fake_log_fs_t *fake);

#endif
