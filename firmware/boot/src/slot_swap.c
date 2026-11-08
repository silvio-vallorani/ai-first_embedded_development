#include "slot_swap.h"

#include <stddef.h>

static bool backend_is_valid(const aied_swap_backend_t *backend)
{
    return (backend != NULL) && (backend->load_journal != NULL) &&
           (backend->store_journal != NULL) && (backend->swap_sector != NULL) &&
           (backend->verify_pair != NULL) && (backend->commit_pair != NULL);
}

static bool swap_core(const aied_swap_backend_t *backend,
                      aied_swap_journal_t *journal,
                      uint32_t core_id,
                      uint32_t total,
                      uint32_t *done)
{
    while (*done < total) {
        if (!backend->swap_sector(backend->context, core_id, *done)) {
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

bool aied_slot_swap_execute(const aied_swap_backend_t *backend)
{
    aied_swap_journal_t journal = {0};

    if (!backend_is_valid(backend) || !backend->load_journal(backend->context, &journal)) {
        return false;
    }

    if (journal.state == AIED_SWAP_COMMITTED) {
        return true;
    }

    if (journal.state == AIED_SWAP_IDLE) {
        journal.state = AIED_SWAP_IN_PROGRESS;
        journal.cm33_sectors_done = 0U;
        journal.cm7_sectors_done = 0U;
        journal.generation++;
        if (!backend->store_journal(backend->context, &journal)) {
            return false;
        }
    }

    if ((journal.cm33_sectors_done > journal.cm33_sectors_total) ||
        (journal.cm7_sectors_done > journal.cm7_sectors_total)) {
        return false;
    }

    if (journal.state == AIED_SWAP_IN_PROGRESS) {
        if (!swap_core(backend, &journal, 0U, journal.cm33_sectors_total,
                       &journal.cm33_sectors_done) ||
            !swap_core(backend, &journal, 1U, journal.cm7_sectors_total,
                       &journal.cm7_sectors_done)) {
            return false;
        }

        if (!backend->verify_pair(backend->context)) {
            return false;
        }

        journal.state = AIED_SWAP_IMAGES_VERIFIED;
        journal.generation++;
        if (!backend->store_journal(backend->context, &journal)) {
            return false;
        }
    } else if (journal.state != AIED_SWAP_IMAGES_VERIFIED) {
        return false;
    }

    /* The pair commit operation must be idempotent across a reset. */
    if (!backend->commit_pair(backend->context, journal.generation)) {
        return false;
    }

    journal.state = AIED_SWAP_COMMITTED;
    journal.generation++;
    return backend->store_journal(backend->context, &journal);
}
