#include "test.h"
#include "aied_log.h"

#include <string.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

typedef struct {
    char text[4096];
    size_t length;
    unsigned lines;
    unsigned flushes;
} capture_t;

static uint64_t s_now_us;
static int s_lock_depth;
static unsigned s_lock_calls;

static uint64_t fake_now(void)
{
    return s_now_us;
}

static uint32_t fake_lock(void)
{
    s_lock_depth++;
    s_lock_calls++;
    return 0x5AU;
}

static void fake_unlock(uint32_t key)
{
    if (key == 0x5AU) {
        s_lock_depth--;
    }
}

static void capture_write(void *context, const char *line, size_t length)
{
    capture_t *capture = context;
    if ((capture->length + length) < sizeof(capture->text)) {
        (void)memcpy(&capture->text[capture->length], line, length);
        capture->length += length;
        capture->text[capture->length] = '\0';
    }
    capture->lines++;
}

static void capture_flush(void *context)
{
    ((capture_t *)context)->flushes++;
}

static const aied_log_config_t s_config = {fake_now, fake_lock, fake_unlock};
static capture_t s_console;
static capture_t s_file;
static const aied_log_backend_t s_console_backend = {capture_write, NULL, &s_console, false};
static const aied_log_backend_t s_file_backend = {capture_write, capture_flush, &s_file, true};

static int setup(bool with_file)
{
    (void)memset(&s_console, 0, sizeof(s_console));
    (void)memset(&s_file, 0, sizeof(s_file));
    s_now_us = 0U;
    TEST_ASSERT_TRUE(aied_log_init(&s_config));
    TEST_ASSERT_TRUE(aied_log_backend_add(&s_console_backend));
    if (with_file) {
        TEST_ASSERT_TRUE(aied_log_backend_add(&s_file_backend));
    }
    return 0;
}

static int test_line_format(void)
{
    char line[AIED_LOG_LINE_MAX];
    const char expected[] = "[01:02:03.004,005] <inf> app: hello\n";
    size_t length;

    length = aied_log_format_line(line, 3723004005ULL, "app", LOG_LEVEL_INF, "hello");
    TEST_ASSERT_EQ(sizeof(expected) - 1U, length);
    TEST_ASSERT_TRUE(memcmp(line, expected, length) == 0);

    /* Each record is one line: inner breaks become spaces, a trailing break is dropped. */
    length = aied_log_format_line(line, 0U, "m", LOG_LEVEL_ERR, "a\nb\r\n");
    TEST_ASSERT_TRUE(memcmp(line, "[00:00:00.000,000] <err> m: a b\n", length) == 0);
    length = aied_log_format_line(line, 0U, "m", 9U, "x");
    TEST_ASSERT_TRUE(memcmp(line, "[00:00:00.000,000] <unk> m: x\n", length) == 0);
    /* Hours keep counting beyond 99. */
    length = aied_log_format_line(line, 360000000000ULL, "m", LOG_LEVEL_DBG, "x");
    TEST_ASSERT_TRUE(memcmp(line, "[100:00:00.000,000] <dbg> m: x\n", length) == 0);
    return 0;
}

static int test_long_message_is_truncated_on_one_line(void)
{
    char message[400];
    char line[AIED_LOG_LINE_MAX];
    size_t length;

    (void)memset(message, 'x', sizeof(message) - 1U);
    message[sizeof(message) - 1U] = '\0';
    length = aied_log_format_line(line, 0U, "app", LOG_LEVEL_WRN, message);
    TEST_ASSERT_EQ(AIED_LOG_LINE_MAX, length);
    TEST_ASSERT_TRUE(memcmp(&line[length - 4U], "...\n", 4U) == 0);
    TEST_ASSERT_TRUE(memchr(line, '\n', length - 1U) == NULL);

    /* A message that fits exactly is not marked as truncated. */
    length = aied_log_format_line(line, 0U, "app", LOG_LEVEL_WRN, "fits");
    TEST_ASSERT_TRUE(memcmp(&line[length - 5U], "fits\n", 5U) == 0);
    return 0;
}

static int test_levels_are_filtered_per_module(void)
{
    TEST_ASSERT_EQ(0, setup(false));
    s_now_us = 1500U;
    LOG_ERR("e%d", 1);
    LOG_WRN("w");
    LOG_INF("i %s", "ok");
    LOG_DBG("hidden");
    TEST_ASSERT_EQ(3U, s_console.lines);
    TEST_ASSERT_TRUE(strcmp(s_console.text, "[00:00:00.001,500] <err> app: e1\n"
                                            "[00:00:00.001,500] <wrn> app: w\n"
                                            "[00:00:00.001,500] <inf> app: i ok\n") == 0);
    return 0;
}

static int test_deferred_backend_receives_records_on_process(void)
{
    TEST_ASSERT_EQ(0, setup(true));
    LOG_INF("one");
    LOG_INF("two");
    TEST_ASSERT_EQ(2U, s_console.lines);
    TEST_ASSERT_EQ(0U, s_file.lines);

    TEST_ASSERT_EQ(2U, aied_log_process());
    TEST_ASSERT_EQ(2U, s_file.lines);
    TEST_ASSERT_EQ(1U, s_file.flushes);
    TEST_ASSERT_TRUE(strcmp(s_file.text, s_console.text) == 0);

    TEST_ASSERT_EQ(0U, aied_log_process());
    TEST_ASSERT_EQ(1U, s_file.flushes);
    TEST_ASSERT_EQ(0, s_lock_depth);
    TEST_ASSERT_TRUE(s_lock_calls > 0U);
    return 0;
}

static int test_full_buffer_drops_and_reports(void)
{
    unsigned written = 0U;

    TEST_ASSERT_EQ(0, setup(true));
    for (unsigned i = 0U; i < 200U; i++) {
        LOG_INF("filler message number %u", i);
    }
    TEST_ASSERT_TRUE(aied_log_dropped() > 0U);
    written = (unsigned)aied_log_process();
    TEST_ASSERT_EQ(0U, aied_log_dropped());
    TEST_ASSERT_TRUE(written < 200U);
    TEST_ASSERT_TRUE(strstr(s_file.text, "<wrn> log: ") != NULL);
    TEST_ASSERT_TRUE(strstr(s_file.text, " messages dropped\n") != NULL);
    /* The buffer works again after draining. */
    LOG_INF("after");
    TEST_ASSERT_EQ(1U, aied_log_process());
    TEST_ASSERT_EQ(0, s_lock_depth);
    return 0;
}

static int test_invalid_use_is_rejected(void)
{
    static const aied_log_config_t no_clock = {NULL, NULL, NULL};
    static const aied_log_backend_t no_write = {NULL, NULL, NULL, false};

    TEST_ASSERT_FALSE(aied_log_init(NULL));
    TEST_ASSERT_FALSE(aied_log_init(&no_clock));
    TEST_ASSERT_EQ(0, setup(false));
    TEST_ASSERT_FALSE(aied_log_backend_add(NULL));
    TEST_ASSERT_FALSE(aied_log_backend_add(&no_write));
    TEST_ASSERT_TRUE(aied_log_backend_add(&s_file_backend));
    TEST_ASSERT_TRUE(aied_log_backend_add(&s_file_backend));
    TEST_ASSERT_TRUE(aied_log_backend_add(&s_file_backend));
    TEST_ASSERT_FALSE(aied_log_backend_add(&s_file_backend));
    aied_log_write(NULL, LOG_LEVEL_ERR, "x");
    TEST_ASSERT_EQ(0U, s_console.lines);
    return 0;
}

int test_log(void)
{
    int failures = 0;
    failures += test_line_format();
    failures += test_long_message_is_truncated_on_one_line();
    failures += test_levels_are_filtered_per_module();
    failures += test_deferred_backend_receives_records_on_process();
    failures += test_full_buffer_drops_and_reports();
    failures += test_invalid_use_is_rejected();
    return failures;
}
