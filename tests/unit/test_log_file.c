#include "test.h"
#include "aied_log_file.h"
#include "fake_log_fs.h"

#include <string.h>

static fake_log_fs_t s_fake;
static aied_log_fs_t s_fs;
static aied_log_file_t s_file;

typedef struct {
    char names[FAKE_LOG_FS_MAX_FILES][AIED_LOG_FILE_NAME_MAX];
    unsigned files;
    char text[8192];
    size_t length;
} dump_t;

static void dump_begin(void *context, const char *name, uint32_t size)
{
    dump_t *dump = context;
    (void)size;
    (void)memcpy(dump->names[dump->files++], name, AIED_LOG_FILE_NAME_MAX);
}

static void dump_data(void *context, const char *data, size_t length)
{
    dump_t *dump = context;
    (void)memcpy(&dump->text[dump->length], data, length);
    dump->length += length;
}

/* Write one 40-byte line, as the core would. */
static void write_line(char tag)
{
    char line[40];
    (void)memset(line, tag, sizeof(line) - 1U);
    line[sizeof(line) - 1U] = '\n';
    s_file.backend.write(s_file.backend.context, line, sizeof(line));
}

static void flush(void)
{
    s_file.backend.flush(s_file.backend.context);
}

static int test_first_boot_creates_counter_and_first_file(void)
{
    fake_log_fs_init(&s_fake);
    s_fs = fake_log_fs_interface(&s_fake);
    TEST_ASSERT_TRUE(aied_log_file_init(&s_file, &s_fs, 200U, 3U));
    TEST_ASSERT_EQ(1U, aied_log_file_boot_count(&s_file));
    TEST_ASSERT_TRUE(aied_log_file_backend(&s_file)->deferred);
    TEST_ASSERT_EQ(0U, fake_log_fs_log_files(&s_fake));

    write_line('a');
    write_line('b');
    TEST_ASSERT_EQ(0U, s_fake.append_calls);
    flush();
    TEST_ASSERT_EQ(1U, s_fake.append_calls);
    TEST_ASSERT_EQ(80U, fake_log_fs_find(&s_fake, "log.00000001.txt")->size);
    flush();
    TEST_ASSERT_EQ(1U, s_fake.append_calls);
    return 0;
}

static int test_rotation_keeps_newest_files(void)
{
    TEST_ASSERT_EQ(0, test_first_boot_creates_counter_and_first_file());
    for (char tag = 'c'; tag <= 'z'; tag++) {
        write_line(tag);
        write_line(tag);
        flush();
    }
    TEST_ASSERT_EQ(3U, fake_log_fs_log_files(&s_fake));
    TEST_ASSERT_TRUE(fake_log_fs_find(&s_fake, "log.00000001.txt") == NULL);
    for (uint32_t i = 0U; i < FAKE_LOG_FS_MAX_FILES; i++) {
        if (s_fake.files[i].used && (strncmp(s_fake.files[i].name, "log.", 4U) == 0)) {
            TEST_ASSERT_TRUE(s_fake.files[i].size <= 200U);
            TEST_ASSERT_EQ('\n', s_fake.files[i].data[s_fake.files[i].size - 1U]);
        }
    }
    return 0;
}

static int test_reboot_resumes_newest_file_and_counts_boots(void)
{
    uint32_t size_before;

    TEST_ASSERT_EQ(0, test_first_boot_creates_counter_and_first_file());
    size_before = fake_log_fs_find(&s_fake, "log.00000001.txt")->size;

    TEST_ASSERT_TRUE(aied_log_file_init(&s_file, &s_fs, 200U, 3U));
    TEST_ASSERT_EQ(2U, aied_log_file_boot_count(&s_file));
    write_line('r');
    flush();
    TEST_ASSERT_EQ(size_before + 40U, fake_log_fs_find(&s_fake, "log.00000001.txt")->size);
    TEST_ASSERT_TRUE(aied_log_file_init(&s_file, &s_fs, 200U, 3U));
    TEST_ASSERT_EQ(3U, aied_log_file_boot_count(&s_file));
    return 0;
}

static int test_dump_streams_files_oldest_first(void)
{
    static dump_t dump;
    const aied_log_dump_sink_t sink = {dump_begin, dump_data, NULL, &dump};

    (void)memset(&dump, 0, sizeof(dump));
    TEST_ASSERT_EQ(0, test_rotation_keeps_newest_files());
    write_line('!'); /* still buffered: the dump must flush it first */
    TEST_ASSERT_TRUE(aied_log_file_dump(&s_file, &sink));
    TEST_ASSERT_EQ(3U, dump.files);
    TEST_ASSERT_TRUE(strcmp(dump.names[0], dump.names[1]) < 0);
    TEST_ASSERT_TRUE(strcmp(dump.names[1], dump.names[2]) < 0);
    TEST_ASSERT_EQ('!', dump.text[dump.length - 2U]);
    TEST_ASSERT_EQ('\n', dump.text[dump.length - 1U]);
    return 0;
}

static int test_write_failure_drops_batch_without_growth(void)
{
    TEST_ASSERT_EQ(0, test_first_boot_creates_counter_and_first_file());
    s_fake.fail_append = true;
    for (int i = 0; i < 30; i++) {
        write_line('f');
    }
    flush();
    TEST_ASSERT_TRUE(aied_log_file_write_errors(&s_file) > 0U);
    /* Persistent failures rotate at most once: an empty new file never rotates again. */
    TEST_ASSERT_EQ(2U, s_file.last_index);
    s_fake.fail_append = false;
    write_line('g');
    flush();
    TEST_ASSERT_EQ(80U, fake_log_fs_find(&s_fake, "log.00000001.txt")->size);
    TEST_ASSERT_EQ(40U, fake_log_fs_find(&s_fake, "log.00000002.txt")->size);
    return 0;
}

static int test_invalid_configuration_is_rejected(void)
{
    aied_log_fs_t broken;

    fake_log_fs_init(&s_fake);
    s_fs = fake_log_fs_interface(&s_fake);
    TEST_ASSERT_FALSE(aied_log_file_init(NULL, &s_fs, 200U, 3U));
    TEST_ASSERT_FALSE(aied_log_file_init(&s_file, NULL, 200U, 3U));
    TEST_ASSERT_FALSE(aied_log_file_init(&s_file, &s_fs, 10U, 3U));
    TEST_ASSERT_FALSE(aied_log_file_init(&s_file, &s_fs, 200U, 0U));
    broken = s_fs;
    broken.remove = NULL;
    TEST_ASSERT_FALSE(aied_log_file_init(&s_file, &broken, 200U, 3U));
    s_fake.fail_list = true;
    TEST_ASSERT_FALSE(aied_log_file_init(&s_file, &s_fs, 200U, 3U));
    return 0;
}

int test_log_file(void)
{
    int failures = 0;
    failures += test_first_boot_creates_counter_and_first_file();
    failures += test_rotation_keeps_newest_files();
    failures += test_reboot_resumes_newest_file_and_counts_boots();
    failures += test_dump_streams_files_oldest_first();
    failures += test_write_failure_drops_batch_without_growth();
    failures += test_invalid_configuration_is_rejected();
    return failures;
}
