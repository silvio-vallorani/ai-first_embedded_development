#include "test.h"
#include "mock_swap_backend.h"

#define CM33_SECTORS 2U
#define CM7_SECTORS  3U

static mock_swap_backend_t mock;
static aied_swap_backend_t backend;

static void setup(void)
{
    mock_swap_backend_init(&mock, CM33_SECTORS, CM7_SECTORS);
    backend = mock_swap_backend_interface(&mock);
}

/* Bring a fresh fake to the TRIAL state of a valid candidate. */
static int setup_trial(void)
{
    setup();
    TEST_ASSERT_TRUE(aied_slot_swap_prepare(&backend, CM33_SECTORS, CM7_SECTORS));
    TEST_ASSERT_EQ(AIED_BOOT_TRIAL, aied_slot_swap_boot(&backend, 1U));
    return 0;
}

static int test_blank_journal_boots_normally(void)
{
    setup();
    TEST_ASSERT_EQ(AIED_BOOT_NORMAL, aied_slot_swap_boot(&backend, 1U));
    TEST_ASSERT_EQ(0U, mock.operation_count);
    return 0;
}

static int test_update_is_swapped_trialled_and_committed(void)
{
    setup();
    TEST_ASSERT_TRUE(aied_slot_swap_prepare(&backend, CM33_SECTORS, CM7_SECTORS));
    TEST_ASSERT_EQ(AIED_SWAP_PREPARED, mock.journal.state);
    TEST_ASSERT_TRUE(mock_swap_backend_all_sectors(&mock, false));

    TEST_ASSERT_EQ(AIED_BOOT_TRIAL, aied_slot_swap_boot(&backend, 1U));
    TEST_ASSERT_TRUE(mock_swap_backend_all_sectors(&mock, true));
    TEST_ASSERT_EQ(AIED_SWAP_TRIAL, mock.journal.state);
    TEST_ASSERT_EQ(1U, mock.journal.trial_boots);
    TEST_ASSERT_EQ(0U, mock.commit_count);

    TEST_ASSERT_TRUE(aied_slot_swap_confirm(&backend));
    TEST_ASSERT_EQ(AIED_SWAP_COMMITTED, mock.journal.state);
    TEST_ASSERT_EQ(1U, mock.commit_count);

    TEST_ASSERT_EQ(AIED_BOOT_NORMAL, aied_slot_swap_boot(&backend, 1U));
    TEST_ASSERT_TRUE(aied_slot_swap_confirm(&backend));
    TEST_ASSERT_EQ(1U, mock.commit_count);
    TEST_ASSERT_TRUE(mock_swap_backend_all_sectors(&mock, true));
    return 0;
}

static int test_unconfirmed_trial_reverts_after_budget(void)
{
    setup();
    TEST_ASSERT_TRUE(aied_slot_swap_prepare(&backend, CM33_SECTORS, CM7_SECTORS));
    TEST_ASSERT_EQ(AIED_BOOT_TRIAL, aied_slot_swap_boot(&backend, 3U));
    TEST_ASSERT_EQ(AIED_BOOT_TRIAL, aied_slot_swap_boot(&backend, 3U));
    TEST_ASSERT_EQ(AIED_BOOT_TRIAL, aied_slot_swap_boot(&backend, 3U));
    TEST_ASSERT_EQ(3U, mock.journal.trial_boots);

    TEST_ASSERT_EQ(AIED_BOOT_NORMAL, aied_slot_swap_boot(&backend, 3U));
    TEST_ASSERT_EQ(AIED_SWAP_REVERTED, mock.journal.state);
    TEST_ASSERT_TRUE(mock_swap_backend_all_sectors(&mock, false));
    TEST_ASSERT_EQ(0U, mock.commit_count);
    TEST_ASSERT_FALSE(aied_slot_swap_confirm(&backend));
    return 0;
}

static int test_corrupt_candidate_is_reverted(void)
{
    setup();
    mock.candidate_corrupt = true;
    TEST_ASSERT_TRUE(aied_slot_swap_prepare(&backend, CM33_SECTORS, CM7_SECTORS));
    TEST_ASSERT_EQ(AIED_BOOT_NORMAL, aied_slot_swap_boot(&backend, 1U));
    TEST_ASSERT_EQ(AIED_SWAP_REVERTED, mock.journal.state);
    TEST_ASSERT_TRUE(mock_swap_backend_all_sectors(&mock, false));
    TEST_ASSERT_EQ(0U, mock.commit_count);
    return 0;
}

static int test_no_valid_pair_enters_recovery(void)
{
    setup();
    mock.candidate_corrupt = true;
    mock.previous_corrupt = true;
    TEST_ASSERT_TRUE(aied_slot_swap_prepare(&backend, CM33_SECTORS, CM7_SECTORS));
    TEST_ASSERT_EQ(AIED_BOOT_RECOVERY, aied_slot_swap_boot(&backend, 1U));
    TEST_ASSERT_EQ(AIED_SWAP_REVERTING, mock.journal.state);
    TEST_ASSERT_EQ(AIED_BOOT_RECOVERY, aied_slot_swap_boot(&backend, 1U));
    return 0;
}

static int test_invalid_inputs_are_rejected(void)
{
    setup();
    TEST_ASSERT_EQ(AIED_BOOT_RECOVERY, aied_slot_swap_boot(NULL, 1U));
    TEST_ASSERT_EQ(AIED_BOOT_RECOVERY, aied_slot_swap_boot(&backend, 0U));
    TEST_ASSERT_FALSE(aied_slot_swap_prepare(NULL, 1U, 1U));
    TEST_ASSERT_FALSE(aied_slot_swap_prepare(&backend, 0U, 1U));
    TEST_ASSERT_FALSE(aied_slot_swap_prepare(&backend, 1U, 0U));
    TEST_ASSERT_FALSE(aied_slot_swap_confirm(NULL));
    TEST_ASSERT_FALSE(aied_slot_swap_confirm(&backend));

    backend.commit_pair = NULL;
    TEST_ASSERT_EQ(AIED_BOOT_RECOVERY, aied_slot_swap_boot(&backend, 1U));

    setup();
    mock.journal_corrupt = true;
    TEST_ASSERT_EQ(AIED_BOOT_RECOVERY, aied_slot_swap_boot(&backend, 1U));
    TEST_ASSERT_FALSE(aied_slot_swap_prepare(&backend, 1U, 1U));

    setup();
    mock.journal.state = (uint32_t)AIED_SWAP_REVERTED + 1U;
    TEST_ASSERT_EQ(AIED_BOOT_RECOVERY, aied_slot_swap_boot(&backend, 1U));

    setup();
    mock.journal.state = (uint32_t)AIED_SWAP_SWAPPING;
    mock.journal.cm7_sectors_total = 1U;
    mock.journal.cm7_sectors_done = 2U;
    TEST_ASSERT_EQ(AIED_BOOT_RECOVERY, aied_slot_swap_boot(&backend, 1U));
    return 0;
}

static int test_prepare_only_from_terminal_state(void)
{
    TEST_ASSERT_EQ(0, setup_trial());
    TEST_ASSERT_FALSE(aied_slot_swap_prepare(&backend, CM33_SECTORS, CM7_SECTORS));
    TEST_ASSERT_TRUE(aied_slot_swap_confirm(&backend));
    TEST_ASSERT_TRUE(aied_slot_swap_prepare(&backend, CM33_SECTORS, CM7_SECTORS));
    TEST_ASSERT_EQ(AIED_SWAP_PREPARED, mock.journal.state);
    TEST_ASSERT_EQ(0U, mock.journal.trial_boots);

    setup();
    mock.journal.state = (uint32_t)AIED_SWAP_REVERTED;
    TEST_ASSERT_TRUE(aied_slot_swap_prepare(&backend, CM33_SECTORS, CM7_SECTORS));
    return 0;
}

static int test_io_failure_resumes_swap(void)
{
    setup();
    TEST_ASSERT_TRUE(aied_slot_swap_prepare(&backend, CM33_SECTORS, CM7_SECTORS));
    mock.cut_at = mock.operation_count + 4U;
    TEST_ASSERT_EQ(AIED_BOOT_RECOVERY, aied_slot_swap_boot(&backend, 1U));
    TEST_ASSERT_EQ(AIED_SWAP_SWAPPING, mock.journal.state);
    TEST_ASSERT_FALSE(mock_swap_backend_all_sectors(&mock, true));
    mock_swap_backend_reboot(&mock);
    TEST_ASSERT_EQ(AIED_BOOT_TRIAL, aied_slot_swap_boot(&backend, 1U));
    TEST_ASSERT_TRUE(mock_swap_backend_all_sectors(&mock, true));
    return 0;
}

/* Cut power at every operation of an uninterrupted forward swap, then reboot. */
static int test_power_cut_at_every_forward_step(void)
{
    uint32_t steps;

    setup();
    TEST_ASSERT_TRUE(aied_slot_swap_prepare(&backend, CM33_SECTORS, CM7_SECTORS));
    steps = mock.operation_count;
    TEST_ASSERT_EQ(AIED_BOOT_TRIAL, aied_slot_swap_boot(&backend, 1U));
    steps = mock.operation_count - steps;
    TEST_ASSERT_TRUE(steps >= (CM33_SECTORS + CM7_SECTORS) * 2U);

    for (uint32_t cut = 1U; cut <= steps; cut++) {
        setup();
        TEST_ASSERT_TRUE(aied_slot_swap_prepare(&backend, CM33_SECTORS, CM7_SECTORS));
        mock.cut_at = mock.operation_count + cut;
        TEST_ASSERT_EQ(AIED_BOOT_RECOVERY, aied_slot_swap_boot(&backend, 1U));
        mock_swap_backend_reboot(&mock);

        TEST_ASSERT_EQ(AIED_BOOT_TRIAL, aied_slot_swap_boot(&backend, 1U));
        TEST_ASSERT_TRUE(mock_swap_backend_all_sectors(&mock, true));
        TEST_ASSERT_EQ(AIED_SWAP_TRIAL, mock.journal.state);
        TEST_ASSERT_EQ(1U, mock.journal.trial_boots);
    }
    return 0;
}

/* Cut power at every operation of a revert caused by a corrupt candidate. */
static int test_power_cut_at_every_revert_step(void)
{
    uint32_t steps;

    setup();
    mock.candidate_corrupt = true;
    TEST_ASSERT_TRUE(aied_slot_swap_prepare(&backend, CM33_SECTORS, CM7_SECTORS));
    steps = mock.operation_count;
    TEST_ASSERT_EQ(AIED_BOOT_NORMAL, aied_slot_swap_boot(&backend, 1U));
    steps = mock.operation_count - steps;

    for (uint32_t cut = 1U; cut <= steps; cut++) {
        setup();
        mock.candidate_corrupt = true;
        TEST_ASSERT_TRUE(aied_slot_swap_prepare(&backend, CM33_SECTORS, CM7_SECTORS));
        mock.cut_at = mock.operation_count + cut;
        TEST_ASSERT_EQ(AIED_BOOT_RECOVERY, aied_slot_swap_boot(&backend, 1U));
        mock_swap_backend_reboot(&mock);

        TEST_ASSERT_EQ(AIED_BOOT_NORMAL, aied_slot_swap_boot(&backend, 1U));
        TEST_ASSERT_EQ(AIED_SWAP_REVERTED, mock.journal.state);
        TEST_ASSERT_TRUE(mock_swap_backend_all_sectors(&mock, false));
        TEST_ASSERT_EQ(0U, mock.commit_count);
    }
    return 0;
}

/* A cut during confirmation must end committed or reverted as a whole pair, never mixed. */
static int test_power_cut_at_every_confirm_step(void)
{
    uint32_t steps;

    TEST_ASSERT_EQ(0, setup_trial());
    steps = mock.operation_count;
    TEST_ASSERT_TRUE(aied_slot_swap_confirm(&backend));
    steps = mock.operation_count - steps;

    for (uint32_t cut = 1U; cut <= steps; cut++) {
        TEST_ASSERT_EQ(0, setup_trial());
        mock.cut_at = mock.operation_count + cut;
        TEST_ASSERT_FALSE(aied_slot_swap_confirm(&backend));
        mock_swap_backend_reboot(&mock);

        TEST_ASSERT_EQ(AIED_BOOT_NORMAL, aied_slot_swap_boot(&backend, 1U));
        if (mock.journal.state == (uint32_t)AIED_SWAP_COMMITTED) {
            TEST_ASSERT_TRUE(mock_swap_backend_all_sectors(&mock, true));
            TEST_ASSERT_TRUE(mock.commit_count >= 1U);
        } else {
            TEST_ASSERT_EQ(AIED_SWAP_REVERTED, mock.journal.state);
            TEST_ASSERT_TRUE(mock_swap_backend_all_sectors(&mock, false));
            TEST_ASSERT_EQ(0U, mock.commit_count);
        }
    }
    return 0;
}

int test_slot_swap(void)
{
    int failures = 0;
    failures += test_blank_journal_boots_normally();
    failures += test_update_is_swapped_trialled_and_committed();
    failures += test_unconfirmed_trial_reverts_after_budget();
    failures += test_corrupt_candidate_is_reverted();
    failures += test_no_valid_pair_enters_recovery();
    failures += test_invalid_inputs_are_rejected();
    failures += test_prepare_only_from_terminal_state();
    failures += test_io_failure_resumes_swap();
    failures += test_power_cut_at_every_forward_step();
    failures += test_power_cut_at_every_revert_step();
    failures += test_power_cut_at_every_confirm_step();
    return failures;
}
