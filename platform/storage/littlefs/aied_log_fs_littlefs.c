#include "aied_log_fs_littlefs.h"

#include <string.h>

static int open_file(aied_log_fs_littlefs_t *fs, lfs_file_t *file, const char *name, int flags)
{
    (void)memset(&fs->file_config, 0, sizeof(fs->file_config));
    fs->file_config.buffer = fs->file_buffer;
    return lfs_file_opencfg(&fs->lfs, file, name, flags, &fs->file_config);
}

static int write_and_close(aied_log_fs_littlefs_t *fs, lfs_file_t *file, const void *data, size_t length)
{
    const lfs_ssize_t written = lfs_file_write(&fs->lfs, file, data, (lfs_size_t)length);
    const int closed = lfs_file_close(&fs->lfs, file); /* close commits the data */

    if (written < 0) {
        return (int)written;
    }
    return ((size_t)written != length) ? LFS_ERR_NOSPC : closed;
}

static int fs_list(void *context, aied_log_fs_name_fn callback, void *callback_context)
{
    aied_log_fs_littlefs_t *fs = context;
    struct lfs_info info;
    lfs_dir_t dir;
    int result = lfs_dir_open(&fs->lfs, &dir, "/");

    if (result < 0) {
        return result;
    }
    while ((result = lfs_dir_read(&fs->lfs, &dir, &info)) > 0) {
        if (info.type == LFS_TYPE_REG) {
            callback(callback_context, info.name);
        }
    }
    (void)lfs_dir_close(&fs->lfs, &dir);
    return (result < 0) ? result : 0;
}

static int fs_size(void *context, const char *name, uint32_t *size)
{
    aied_log_fs_littlefs_t *fs = context;
    struct lfs_info info;
    const int result = lfs_stat(&fs->lfs, name, &info);

    if (result < 0) {
        return result;
    }
    *size = (uint32_t)info.size;
    return 0;
}

static int fs_append(void *context, const char *name, const void *data, size_t length)
{
    aied_log_fs_littlefs_t *fs = context;
    lfs_file_t file;
    const int result = open_file(fs, &file, name, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND);

    return (result < 0) ? result : write_and_close(fs, &file, data, length);
}

static int fs_read(void *context, const char *name, uint32_t offset, void *data, size_t length, size_t *read)
{
    aied_log_fs_littlefs_t *fs = context;
    lfs_file_t file;
    lfs_ssize_t got;
    int result = open_file(fs, &file, name, LFS_O_RDONLY);

    if (result < 0) {
        return result;
    }
    result = (int)lfs_file_seek(&fs->lfs, &file, (lfs_soff_t)offset, LFS_SEEK_SET);
    got = (result < 0) ? (lfs_ssize_t)result : lfs_file_read(&fs->lfs, &file, data, (lfs_size_t)length);
    (void)lfs_file_close(&fs->lfs, &file);
    if (got < 0) {
        return (int)got;
    }
    *read = (size_t)got;
    return 0;
}

static int fs_remove(void *context, const char *name)
{
    return lfs_remove(&((aied_log_fs_littlefs_t *)context)->lfs, name);
}

static int fs_write_file(void *context, const char *name, const void *data, size_t length)
{
    aied_log_fs_littlefs_t *fs = context;
    lfs_file_t file;
    const int result = open_file(fs, &file, name, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);

    return (result < 0) ? result : write_and_close(fs, &file, data, length);
}

int aied_log_fs_littlefs_mount(aied_log_fs_littlefs_t *fs, const struct lfs_config *base)
{
    int result;

    if ((fs == NULL) || (base == NULL) || (base->cache_size > AIED_LOG_FS_CACHE_MAX) ||
        (base->lookahead_size > AIED_LOG_FS_LOOKAHEAD_MAX)) {
        return LFS_ERR_INVAL;
    }
    (void)memset(fs, 0, sizeof(*fs));
    fs->config = *base;
    fs->config.read_buffer = fs->read_buffer;
    fs->config.prog_buffer = fs->prog_buffer;
    fs->config.lookahead_buffer = fs->lookahead_buffer;

    result = lfs_mount(&fs->lfs, &fs->config);
    if ((result == LFS_ERR_CORRUPT) || (result == LFS_ERR_INVAL)) {
        /* No valid file system in the region (first boot or erased flash). */
        result = lfs_format(&fs->lfs, &fs->config);
        if (result == 0) {
            fs->formatted = true;
            result = lfs_mount(&fs->lfs, &fs->config);
        }
    }
    return result;
}

aied_log_fs_t aied_log_fs_littlefs_interface(aied_log_fs_littlefs_t *fs)
{
    const aied_log_fs_t interface = {.list = fs_list,
                                     .size = fs_size,
                                     .append = fs_append,
                                     .read = fs_read,
                                     .remove = fs_remove,
                                     .write_file = fs_write_file,
                                     .context = fs};
    return interface;
}
