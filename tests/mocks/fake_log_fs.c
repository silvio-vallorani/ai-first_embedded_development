#include "fake_log_fs.h"

#include <string.h>

fake_log_file_t *fake_log_fs_find(fake_log_fs_t *fake, const char *name)
{
    for (uint32_t i = 0U; i < FAKE_LOG_FS_MAX_FILES; i++) {
        if (fake->files[i].used && (strcmp(fake->files[i].name, name) == 0)) {
            return &fake->files[i];
        }
    }
    return NULL;
}

static fake_log_file_t *create(fake_log_fs_t *fake, const char *name)
{
    for (uint32_t i = 0U; i < FAKE_LOG_FS_MAX_FILES; i++) {
        if (!fake->files[i].used) {
            const size_t length = strlen(name);
            (void)memset(&fake->files[i], 0, sizeof(fake->files[i]));
            fake->files[i].used = true;
            (void)memcpy(fake->files[i].name, name,
                         (length < AIED_LOG_FILE_NAME_MAX) ? length : (AIED_LOG_FILE_NAME_MAX - 1U));
            return &fake->files[i];
        }
    }
    return NULL;
}

static int fake_list(void *context, aied_log_fs_name_fn callback, void *callback_context)
{
    fake_log_fs_t *fake = context;
    if (fake->fail_list) {
        return -1;
    }
    for (uint32_t i = 0U; i < FAKE_LOG_FS_MAX_FILES; i++) {
        if (fake->files[i].used) {
            callback(callback_context, fake->files[i].name);
        }
    }
    return 0;
}

static int fake_size(void *context, const char *name, uint32_t *size)
{
    const fake_log_file_t *file = fake_log_fs_find(context, name);
    if (file == NULL) {
        return -2;
    }
    *size = file->size;
    return 0;
}

static int fake_append(void *context, const char *name, const void *data, size_t length)
{
    fake_log_fs_t *fake = context;
    fake_log_file_t *file;

    fake->append_calls++;
    if (fake->fail_append) {
        return -5;
    }
    file = fake_log_fs_find(fake, name);
    if (file == NULL) {
        file = create(fake, name);
    }
    if ((file == NULL) || ((file->size + length) > FAKE_LOG_FS_FILE_SIZE)) {
        return -28;
    }
    (void)memcpy(&file->data[file->size], data, length);
    file->size += (uint32_t)length;
    return 0;
}

static int fake_read(void *context, const char *name, uint32_t offset, void *data, size_t length, size_t *read)
{
    const fake_log_file_t *file = fake_log_fs_find(context, name);
    size_t available;

    if ((file == NULL) || (offset > file->size)) {
        return -2;
    }
    available = file->size - offset;
    *read = (length < available) ? length : available;
    (void)memcpy(data, &file->data[offset], *read);
    return 0;
}

static int fake_remove(void *context, const char *name)
{
    fake_log_file_t *file = fake_log_fs_find(context, name);
    if (file == NULL) {
        return -2;
    }
    file->used = false;
    return 0;
}

static int fake_write_file(void *context, const char *name, const void *data, size_t length)
{
    fake_log_fs_t *fake = context;
    fake_log_file_t *file = fake_log_fs_find(fake, name);

    if (file == NULL) {
        file = create(fake, name);
    }
    if ((file == NULL) || (length > FAKE_LOG_FS_FILE_SIZE)) {
        return -28;
    }
    (void)memcpy(file->data, data, length);
    file->size = (uint32_t)length;
    return 0;
}

void fake_log_fs_init(fake_log_fs_t *fake)
{
    (void)memset(fake, 0, sizeof(*fake));
}

aied_log_fs_t fake_log_fs_interface(fake_log_fs_t *fake)
{
    const aied_log_fs_t fs = {.list = fake_list,
                              .size = fake_size,
                              .append = fake_append,
                              .read = fake_read,
                              .remove = fake_remove,
                              .write_file = fake_write_file,
                              .context = fake};
    return fs;
}

uint32_t fake_log_fs_log_files(const fake_log_fs_t *fake)
{
    uint32_t count = 0U;
    for (uint32_t i = 0U; i < FAKE_LOG_FS_MAX_FILES; i++) {
        if (fake->files[i].used && (strncmp(fake->files[i].name, "log.", 4U) == 0)) {
            count++;
        }
    }
    return count;
}
