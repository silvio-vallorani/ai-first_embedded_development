#ifndef AIED_TEST_H
#define AIED_TEST_H

#include <stdio.h>

#define TEST_ASSERT_TRUE(expr) do { if (!(expr)) { \
    (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)
#define TEST_ASSERT_FALSE(expr) TEST_ASSERT_TRUE(!(expr))
#define TEST_ASSERT_EQ(expected, actual) TEST_ASSERT_TRUE((expected) == (actual))

int test_release_manifest(void);
int test_slot_swap(void);

#endif
