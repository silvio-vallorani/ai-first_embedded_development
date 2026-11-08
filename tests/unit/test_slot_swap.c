#include "test.h"
#include "mock_swap_backend.h"

int test_slot_swap(void)
{
    mock_swap_backend_t mock;
    mock_swap_backend_init(&mock, 2U, 3U);
    aied_swap_backend_t backend = mock_swap_backend_interface(&mock);

    TEST_ASSERT_TRUE(aied_slot_swap_execute(&backend));
    TEST_ASSERT_EQ(2U, mock.swapped_cm33);
    TEST_ASSERT_EQ(3U, mock.swapped_cm7);
    TEST_ASSERT_TRUE(mock.committed);
    TEST_ASSERT_EQ(AIED_SWAP_COMMITTED, mock.journal.state);

    mock_swap_backend_init(&mock, 2U, 3U);
    backend = mock_swap_backend_interface(&mock);
    mock.fail_on_operation = 2U;
    TEST_ASSERT_FALSE(aied_slot_swap_execute(&backend));
    TEST_ASSERT_FALSE(mock.committed);
    TEST_ASSERT_EQ(AIED_SWAP_IN_PROGRESS, mock.journal.state);

    mock.fail_on_operation = 0U;
    TEST_ASSERT_TRUE(aied_slot_swap_execute(&backend));
    TEST_ASSERT_EQ(2U, mock.swapped_cm33);
    TEST_ASSERT_EQ(3U, mock.swapped_cm7);
    TEST_ASSERT_TRUE(mock.committed);

    mock_swap_backend_init(&mock, 1U, 1U);
    backend = mock_swap_backend_interface(&mock);
    mock.fail_commit = true;
    TEST_ASSERT_FALSE(aied_slot_swap_execute(&backend));
    TEST_ASSERT_EQ(AIED_SWAP_IMAGES_VERIFIED, mock.journal.state);
    mock.fail_commit = false;
    TEST_ASSERT_TRUE(aied_slot_swap_execute(&backend));
    TEST_ASSERT_TRUE(mock.committed);
    TEST_ASSERT_EQ(AIED_SWAP_COMMITTED, mock.journal.state);
    return 0;
}
