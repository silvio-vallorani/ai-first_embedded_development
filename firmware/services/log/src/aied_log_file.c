#include "aied_log_file.h"

#include <stdio.h>
#include <string.h>

#define BOOT_COUNT_FILE "boot.cnt"
#define NAME_PREFIX     "log."
#define NAME_SUFFIX     ".txt"
#define NAME_DIGITS     8U

static void file_name(char *name, uint32_t index)
{
    (void)snprintf(name, AIED_LOG_FILE_NAME_MAX, NAME_PREFIX "%08lu" NAME_SUFFIX, (unsigned long)index);
}

/* Parse "log.NNNNNNNN.txt"; returns false for any other name. */
static bool parse_name(const char *name, uint32_t *index)
{
    const size_t prefix = sizeof(NAME_PREFIX) - 1U;
    uint32_t value = 0U;

    if ((strlen(name) != (prefix + NAME_DIGITS + sizeof(NAME_SUFFIX) - 1U)) ||
        (strncmp(name, NAME_PREFIX, prefix) != 0) || (strcmp(&name[prefix + NAME_DIGITS], NAME_SUFFIX) != 0)) {
        return false;
    }
    for (size_t i = prefix; i < (prefix + NAME_DIGITS); i++) {
        if ((name[i] < '0') || (name[i] > '9')) {
            return false;
        }
        value = (value * 10U) + (uint32_t)(name[i] - '0');
    }
    *index = value;
    return true;
}

static void scan_callback(void *context, const char *name)
{
    aied_log_file_t *file = context;
    uint32_t index;

    if (!parse_name(name, &index)) {
        return;
    }
    if (!file->has_files) {
        file->first_index = index;
        file->last_index = index;
        file->has_files = true;
    } else {
        file->first_index = (index < file->first_index) ? index : file->first_index;
        file->last_index = (index > file->last_index) ? index : file->last_index;
    }
}

static void remove_excess_files(aied_log_file_t *file)
{
    char name[AIED_LOG_FILE_NAME_MAX];

    while ((file->last_index - file->first_index + 1U) > file->max_files) {
        file_name(name, file->first_index);
        (void)file->fs->remove(file->fs->context, name); /* A missing file in a gap is fine. */
        file->first_index++;
    }
}

static void flush(void *context)
{
    aied_log_file_t *file = context;
    char name[AIED_LOG_FILE_NAME_MAX];

    if (file->buffered == 0U) {
        return;
    }
    if ((file->current_size > 0U) && ((file->current_size + file->buffered) > file->max_file_size)) {
        file->last_index++;
        file->current_size = 0U;
        remove_excess_files(file);
    }
    file_name(name, file->last_index);
    if (file->fs->append(file->fs->context, name, file->buffer, file->buffered) == 0) {
        file->current_size += (uint32_t)file->buffered;
        file->has_files = true;
    } else {
        file->write_errors++; /* Drop the batch rather than grow without bound. */
    }
    file->buffered = 0U;
}

static void write_line(void *context, const char *line, size_t length)
{
    aied_log_file_t *file = context;

    if ((file->buffered + length) > sizeof(file->buffer)) {
        flush(file);
    }
    if (length <= sizeof(file->buffer)) {
        (void)memcpy(&file->buffer[file->buffered], line, length);
        file->buffered += length;
    }
}

static void update_boot_count(aied_log_file_t *file)
{
    uint8_t raw[4] = {0U, 0U, 0U, 0U};
    size_t got = 0U;
    uint32_t previous = 0U;

    if ((file->fs->read(file->fs->context, BOOT_COUNT_FILE, 0U, raw, sizeof(raw), &got) == 0) && (got == sizeof(raw))) {
        previous = (uint32_t)raw[0] | ((uint32_t)raw[1] << 8) | ((uint32_t)raw[2] << 16) | ((uint32_t)raw[3] << 24);
    }
    file->boot_count = previous + 1U;
    raw[0] = (uint8_t)file->boot_count;
    raw[1] = (uint8_t)(file->boot_count >> 8);
    raw[2] = (uint8_t)(file->boot_count >> 16);
    raw[3] = (uint8_t)(file->boot_count >> 24);
    (void)file->fs->write_file(file->fs->context, BOOT_COUNT_FILE, raw, sizeof(raw));
}

bool aied_log_file_init(aied_log_file_t *file, const aied_log_fs_t *fs, uint32_t max_file_size, uint32_t max_files)
{
    char name[AIED_LOG_FILE_NAME_MAX];

    if ((file == NULL) || (fs == NULL) || (fs->list == NULL) || (fs->size == NULL) || (fs->append == NULL) ||
        (fs->read == NULL) || (fs->remove == NULL) || (fs->write_file == NULL) || (max_file_size < AIED_LOG_LINE_MAX) ||
        (max_files == 0U)) {
        return false;
    }
    (void)memset(file, 0, sizeof(*file));
    file->fs = fs;
    file->max_file_size = max_file_size;
    file->max_files = max_files;
    file->first_index = 1U;
    file->last_index = 1U;
    if (fs->list(fs->context, scan_callback, file) != 0) {
        return false;
    }
    if (file->has_files) {
        remove_excess_files(file);
        file_name(name, file->last_index);
        if (fs->size(fs->context, name, &file->current_size) != 0) {
            file->current_size = 0U;
        }
    }
    update_boot_count(file);

    file->backend.write = write_line;
    file->backend.flush = flush;
    file->backend.context = file;
    file->backend.deferred = true;
    return true;
}

const aied_log_backend_t *aied_log_file_backend(aied_log_file_t *file)
{
    return &file->backend;
}

uint32_t aied_log_file_boot_count(const aied_log_file_t *file)
{
    return file->boot_count;
}

uint32_t aied_log_file_write_errors(const aied_log_file_t *file)
{
    return file->write_errors;
}

bool aied_log_file_dump(aied_log_file_t *file, const aied_log_dump_sink_t *sink)
{
    char name[AIED_LOG_FILE_NAME_MAX];
    char chunk[128];
    bool complete = true;

    if ((file == NULL) || (file->fs == NULL) || (sink == NULL) || (sink->data == NULL)) {
        return false;
    }
    flush(file);
    if (!file->has_files) {
        return true;
    }
    for (uint32_t index = file->first_index; index <= file->last_index; index++) {
        uint32_t size = 0U;
        uint32_t offset = 0U;

        file_name(name, index);
        if (file->fs->size(file->fs->context, name, &size) != 0) {
            continue; /* Gap left by a failed write. */
        }
        if (sink->file_begin != NULL) {
            sink->file_begin(sink->context, name, size);
        }
        while (offset < size) {
            size_t got = 0U;
            if ((file->fs->read(file->fs->context, name, offset, chunk, sizeof(chunk), &got) != 0) || (got == 0U)) {
                complete = false;
                break;
            }
            sink->data(sink->context, chunk, got);
            offset += (uint32_t)got;
        }
        if (sink->file_end != NULL) {
            sink->file_end(sink->context, name);
        }
    }
    return complete;
}
