#include "aied_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *const s_level_tags[] = {"none", "err", "wrn", "inf", "dbg"};

static const aied_log_config_t *s_config;
static const aied_log_backend_t *s_backends[AIED_LOG_MAX_BACKENDS];
static size_t s_backend_count;
static bool s_has_deferred;

/* Ring buffer of complete lines, each terminated by '\n'. */
static char s_ring[AIED_LOG_BUFFER_SIZE];
static size_t s_head;
static size_t s_tail;
static size_t s_used;
static uint32_t s_dropped;

static uint32_t lock(void)
{
    return ((s_config != NULL) && (s_config->lock != NULL)) ? s_config->lock() : 0U;
}

static void unlock(uint32_t key)
{
    if ((s_config != NULL) && (s_config->unlock != NULL)) {
        s_config->unlock(key);
    }
}

static void ring_push(const char *line, size_t length)
{
    const uint32_t key = lock();
    if (length > (AIED_LOG_BUFFER_SIZE - s_used)) {
        s_dropped++;
    } else {
        for (size_t i = 0U; i < length; i++) {
            s_ring[s_head] = line[i];
            s_head = (s_head + 1U) % AIED_LOG_BUFFER_SIZE;
        }
        s_used += length;
    }
    unlock(key);
}

static bool ring_pop(char *line, size_t *length)
{
    bool found = false;
    const uint32_t key = lock();
    size_t n = 0U;
    while ((n < s_used) && !found) {
        line[n] = s_ring[(s_tail + n) % AIED_LOG_BUFFER_SIZE];
        found = (line[n] == '\n');
        n++;
    }
    if (found) {
        s_tail = (s_tail + n) % AIED_LOG_BUFFER_SIZE;
        s_used -= n;
        *length = n;
    }
    unlock(key);
    return found;
}

static void dispatch(const char *line, size_t length, bool deferred)
{
    for (size_t i = 0U; i < s_backend_count; i++) {
        if (s_backends[i]->deferred == deferred) {
            s_backends[i]->write(s_backends[i]->context, line, length);
        }
    }
}

size_t aied_log_format_line(char *buffer, uint64_t timestamp_us, const char *module_name, uint32_t level,
                            const char *message)
{
    const uint64_t total_ms = timestamp_us / 1000U;
    const uint64_t total_s = total_ms / 1000U;
    const char *tag = (level < (sizeof(s_level_tags) / sizeof(s_level_tags[0]))) ? s_level_tags[level] : "unk";
    const size_t last = AIED_LOG_LINE_MAX - 1U; /* index reserved for '\n' */
    size_t message_length = strlen(message);
    size_t pos;
    int prefix;

    prefix =
        snprintf(buffer, AIED_LOG_LINE_MAX, "[%02lu:%02u:%02u.%03u,%03u] <%s> %s: ", (unsigned long)(total_s / 3600U),
                 (unsigned)((total_s / 60U) % 60U), (unsigned)(total_s % 60U), (unsigned)(total_ms % 1000U),
                 (unsigned)(timestamp_us % 1000U), tag, module_name);
    pos = (prefix < 0) ? 0U : (((size_t)prefix > last) ? last : (size_t)prefix);

    while ((message_length > 0U) &&
           ((message[message_length - 1U] == '\n') || (message[message_length - 1U] == '\r'))) {
        message_length--;
    }
    size_t copied = 0U;
    for (; (copied < message_length) && (pos < last); copied++) {
        const char c = message[copied];
        buffer[pos++] = ((c == '\n') || (c == '\r')) ? ' ' : c;
    }
    if ((copied < message_length) && (pos >= 3U)) {
        (void)memcpy(&buffer[pos - 3U], "...", 3U);
    }
    buffer[pos++] = '\n';
    return pos;
}

bool aied_log_init(const aied_log_config_t *config)
{
    if ((config == NULL) || (config->now_us == NULL)) {
        return false;
    }
    s_config = config;
    s_backend_count = 0U;
    s_has_deferred = false;
    s_head = 0U;
    s_tail = 0U;
    s_used = 0U;
    s_dropped = 0U;
    return true;
}

bool aied_log_backend_add(const aied_log_backend_t *backend)
{
    if ((s_config == NULL) || (backend == NULL) || (backend->write == NULL) ||
        (s_backend_count >= AIED_LOG_MAX_BACKENDS)) {
        return false;
    }
    s_backends[s_backend_count++] = backend;
    s_has_deferred = s_has_deferred || backend->deferred;
    return true;
}

void aied_log_write(const aied_log_module_t *module, uint32_t level, const char *format, ...)
{
    char message[AIED_LOG_LINE_MAX];
    char line[AIED_LOG_LINE_MAX];
    va_list args;
    size_t length;

    if ((s_config == NULL) || (module == NULL) || (format == NULL)) {
        return;
    }
    va_start(args, format);
    (void)vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    length = aied_log_format_line(line, s_config->now_us(), module->name, level, message);
    dispatch(line, length, false);
    if (s_has_deferred) {
        ring_push(line, length);
    }
}

size_t aied_log_process(void)
{
    static const aied_log_module_t log_module = {"log", LOG_LEVEL_WRN};
    char line[AIED_LOG_LINE_MAX];
    size_t length = 0U;
    size_t count = 0U;
    uint32_t dropped;
    uint32_t key;

    if (s_config == NULL) {
        return 0U;
    }
    while (ring_pop(line, &length)) {
        dispatch(line, length, true);
        count++;
    }

    key = lock();
    dropped = s_dropped;
    s_dropped = 0U;
    unlock(key);
    if (dropped > 0U) {
        char message[48];
        (void)snprintf(message, sizeof(message), "%lu messages dropped", (unsigned long)dropped);
        length = aied_log_format_line(line, s_config->now_us(), log_module.name, LOG_LEVEL_WRN, message);
        dispatch(line, length, true);
        count++;
    }

    if (count > 0U) {
        for (size_t i = 0U; i < s_backend_count; i++) {
            if (s_backends[i]->deferred && (s_backends[i]->flush != NULL)) {
                s_backends[i]->flush(s_backends[i]->context);
            }
        }
    }
    return count;
}

uint32_t aied_log_dropped(void)
{
    const uint32_t key = lock();
    const uint32_t dropped = s_dropped;
    unlock(key);
    return dropped;
}
