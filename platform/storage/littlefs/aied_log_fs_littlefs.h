#ifndef AIED_LOG_FS_LITTLEFS_H
#define AIED_LOG_FS_LITTLEFS_H

/**
 * @file aied_log_fs_littlefs.h
 * @brief aied_log_fs_t implementation on littlefs (root directory), without heap use.
 */

#include "aied_log_file.h"
#include "lfs.h"

#include <stdbool.h>
#include <stdint.h>

/** @brief Largest supported littlefs cache and lookahead sizes (static buffers). */
#define AIED_LOG_FS_CACHE_MAX     256U
#define AIED_LOG_FS_LOOKAHEAD_MAX 64U

/** @brief littlefs instance with its static buffers. */
typedef struct {
    lfs_t lfs;
    struct lfs_config config;
    struct lfs_file_config file_config;
    uint32_t read_buffer[AIED_LOG_FS_CACHE_MAX / 4U];
    uint32_t prog_buffer[AIED_LOG_FS_CACHE_MAX / 4U];
    uint32_t lookahead_buffer[AIED_LOG_FS_LOOKAHEAD_MAX / 4U];
    uint32_t file_buffer[AIED_LOG_FS_CACHE_MAX / 4U];
    bool formatted; /**< true when mount found no valid file system and formatted it. */
} aied_log_fs_littlefs_t;

/**
 * @brief Mount littlefs, formatting the region only if it holds no valid file system.
 * @param fs Instance.
 * @param base Geometry and block-device callbacks (buffers are replaced by static ones).
 * @return 0 on success, a negative littlefs error otherwise.
 */
int aied_log_fs_littlefs_mount(aied_log_fs_littlefs_t *fs, const struct lfs_config *base);

/**
 * @brief Build the log file system interface bound to a mounted instance.
 * @param fs Mounted instance.
 * @return Interface.
 */
aied_log_fs_t aied_log_fs_littlefs_interface(aied_log_fs_littlefs_t *fs);

#endif
