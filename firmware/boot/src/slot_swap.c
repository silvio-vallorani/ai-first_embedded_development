#include "slot_swap.h"

#include <stddef.h>

static bool backend_is_valid(const aied_swap_backend_t *backend)
{
    return (backend != NULL) && (backend->load_journal != NULL) &&
           (backend->store_journal != NULL) && (backend->swap_sector != NULL) &&
           (backend->verify_pair != NULL) && (backend->commit_pair != NULL);
}

static bool load_consistent_journal(const aied_swap_backend_t *backend,
                                    aied_swap_journal_t *journal)
{
    if (!backend_is_valid(backend) || !backend->load_journal(backend->context, journal)) {
        return false;
    }
    return (journal->state <= (uint32_t)AIED_SWAP_REVERTED) &&
           (journal->cm33_sectors_done <= journal->cm33_sectors_total) &&
           (journal->cm7_sectors_done <= journal->cm7_sectors_total);
}

static bool is_terminal(uint32_t state)
{
    return (state == (uint32_t)AIED_SWAP_IDLE) || (state == (uint32_t)AIED_SWAP_COMMITTED) ||
           (state == (uint32_t)AIED_SWAP_REVERTED);
}

static bool store_state(const aied_swap_backend_t *backend,
                        aied_swap_journal_t *journal,
                        aied_swap_state_t state)
{
    journal->state = (uint32_t)state;
    journal->generation++;
    return backend->store_journal(backend->context, journal);
}

static bool swap_core(const aied_swap_backend_t *backend,
                      aied_swap_journal_t *journal,
                      uint32_t core_id,
                      uint32_t total,
                      uint32_t *done,
                      aied_swap_direction_t direction)
{
    while (*done < total) {
        if (!backend->swap_sector(backend->context, core_id, *done, direction)) {
            return false;
        }
        (*done)++;
        journal->generation++;
        if (!backend->store_journal(backend->context, journal)) {
            return false;
        }
    }
    return true;
}

static bool swap_pair(const aied_swap_backend_t *backend,
                      aied_swap_journal_t *journal,
                      aied_swap_direction_t direction)
{
    return swap_core(backend, journal, 0U, journal->cm33_sectors_total,
                     &journal->cm33_sectors_done, direction) &&
           swap_core(backend, journal, 1U, journal->cm7_sectors_total,
                     &journal->cm7_sectors_done, direction);
}

/* Revert always follows a completed forward swap, so every sector is swapped back. */
static aied_boot_action_t revert_pair(const aied_swap_backend_t *backend,
                                      aied_swap_journal_t *journal,
                                      bool restart)
{
    if (restart) {
        journal->cm33_sectors_done = 0U;
        journal->cm7_sectors_done = 0U;
        if (!store_state(backend, journal, AIED_SWAP_REVERTING)) {
            return AIED_BOOT_RECOVERY;
        }
    }
    if (!swap_pair(backend, journal, AIED_SWAP_REVERT) || !backend->verify_pair(backend->context)) {
        return AIED_BOOT_RECOVERY;
    }
    return store_state(backend, journal, AIED_SWAP_REVERTED) ? AIED_BOOT_NORMAL
                                                             : AIED_BOOT_RECOVERY;
}

static bool finish_commit(const aied_swap_backend_t *backend, aied_swap_journal_t *journal)
{
    return backend->commit_pair(backend->context, journal->generation) &&
           store_state(backend, journal, AIED_SWAP_COMMITTED);
}

bool aied_slot_swap_prepare(const aied_swap_backend_t *backend,
                            uint32_t cm33_sectors,
                            uint32_t cm7_sectors)
{
    aied_swap_journal_t journal = {0};

    if ((cm33_sectors == 0U) || (cm7_sectors == 0U) ||
        !load_consistent_journal(backend, &journal) || !is_terminal(journal.state)) {
        return false;
    }
    journal.cm33_sectors_total = cm33_sectors;
    journal.cm33_sectors_done = 0U;
    journal.cm7_sectors_total = cm7_sectors;
    journal.cm7_sectors_done = 0U;
    journal.trial_boots = 0U;
    return store_state(backend, &journal, AIED_SWAP_PREPARED);
}

aied_boot_action_t aied_slot_swap_boot(const aied_swap_backend_t *backend,
                                       uint32_t max_trial_boots)
{
    aied_swap_journal_t journal = {0};

    if ((max_trial_boots == 0U) || !load_consistent_journal(backend, &journal)) {
        return AIED_BOOT_RECOVERY;
    }
    if (is_terminal(journal.state)) {
        return AIED_BOOT_NORMAL;
    }
    if (journal.state == (uint32_t)AIED_SWAP_COMMITTING) {
        return finish_commit(backend, &journal) ? AIED_BOOT_NORMAL : AIED_BOOT_RECOVERY;
    }
    if (journal.state == (uint32_t)AIED_SWAP_REVERTING) {
        return revert_pair(backend, &journal, false);
    }

    if (journal.state == (uint32_t)AIED_SWAP_PREPARED) {
        journal.cm33_sectors_done = 0U;
        journal.cm7_sectors_done = 0U;
        if (!store_state(backend, &journal, AIED_SWAP_SWAPPING)) {
            return AIED_BOOT_RECOVERY;
        }
    }
    if (journal.state == (uint32_t)AIED_SWAP_SWAPPING) {
        /* An I/O failure leaves SWAPPING persisted; the next boot resumes the swap. */
        if (!swap_pair(backend, &journal, AIED_SWAP_FORWARD)) {
            return AIED_BOOT_RECOVERY;
        }
        if (!backend->verify_pair(backend->context)) {
            return revert_pair(backend, &journal, true);
        }
        journal.state = (uint32_t)AIED_SWAP_TRIAL;
        journal.trial_boots = 0U;
    }

    /* TRIAL: each unconfirmed boot consumes one attempt of the probation budget. */
    if (journal.trial_boots >= max_trial_boots) {
        return revert_pair(backend, &journal, true);
    }
    journal.trial_boots++;
    return store_state(backend, &journal, AIED_SWAP_TRIAL) ? AIED_BOOT_TRIAL
                                                           : AIED_BOOT_RECOVERY;
}

bool aied_slot_swap_confirm(const aied_swap_backend_t *backend)
{
    aied_swap_journal_t journal = {0};

    if (!load_consistent_journal(backend, &journal)) {
        return false;
    }
    if (journal.state == (uint32_t)AIED_SWAP_COMMITTED) {
        return true;
    }
    if (journal.state == (uint32_t)AIED_SWAP_TRIAL) {
        if (!store_state(backend, &journal, AIED_SWAP_COMMITTING)) {
            return false;
        }
    } else if (journal.state != (uint32_t)AIED_SWAP_COMMITTING) {
        return false;
    }
    return finish_commit(backend, &journal);
}
