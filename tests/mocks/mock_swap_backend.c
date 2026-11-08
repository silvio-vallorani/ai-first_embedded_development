#include "mock_swap_backend.h"

#include <string.h>

/* Count one side-effecting operation; return false when power is (or becomes) lost. */
static bool mock_power_ok(mock_swap_backend_t *mock)
{
    if (mock->power_lost) {
        return false;
    }
    mock->operation_count++;
    if ((mock->cut_at != 0U) && (mock->operation_count == mock->cut_at)) {
        mock->power_lost = true;
        return false;
    }
    return true;
}

static bool mock_load(void *context, aied_swap_journal_t *journal)
{
    mock_swap_backend_t *mock = context;
    if (mock->journal_corrupt) {
        return false;
    }
    *journal = mock->journal;
    return true;
}

static bool mock_store(void *context, const aied_swap_journal_t *journal)
{
    mock_swap_backend_t *mock = context;
    if (!mock_power_ok(mock)) {
        return false;
    }
    mock->journal = *journal;
    return true;
}

static bool mock_swap(void *context, uint32_t core_id, uint32_t sector_index,
                      aied_swap_direction_t direction)
{
    mock_swap_backend_t *mock = context;
    if ((core_id > 1U) || (sector_index >= mock->sectors[core_id]) || !mock_power_ok(mock)) {
        return false;
    }
    /* Idempotent per direction, as the backend contract requires. */
    mock->sector_swapped[core_id][sector_index] = (direction == AIED_SWAP_FORWARD);
    return true;
}

static bool mock_verify(void *context)
{
    const mock_swap_backend_t *mock = context;
    if (mock_swap_backend_all_sectors(mock, true)) {
        return !mock->candidate_corrupt;
    }
    if (mock_swap_backend_all_sectors(mock, false)) {
        return !mock->previous_corrupt;
    }
    return false; /* Mixed pair. */
}

static bool mock_commit(void *context, uint32_t generation)
{
    mock_swap_backend_t *mock = context;
    (void)generation;
    if (!mock_power_ok(mock)) {
        return false;
    }
    mock->commit_count++;
    return true;
}

void mock_swap_backend_init(mock_swap_backend_t *mock, uint32_t cm33_sectors, uint32_t cm7_sectors)
{
    (void)memset(mock, 0, sizeof(*mock));
    mock->journal.state = (uint32_t)AIED_SWAP_IDLE;
    mock->sectors[0] = cm33_sectors;
    mock->sectors[1] = cm7_sectors;
}

void mock_swap_backend_reboot(mock_swap_backend_t *mock)
{
    mock->power_lost = false;
    mock->cut_at = 0U;
}

bool mock_swap_backend_all_sectors(const mock_swap_backend_t *mock, bool swapped)
{
    for (uint32_t core = 0U; core < 2U; core++) {
        for (uint32_t sector = 0U; sector < mock->sectors[core]; sector++) {
            if (mock->sector_swapped[core][sector] != swapped) {
                return false;
            }
        }
    }
    return true;
}

aied_swap_backend_t mock_swap_backend_interface(mock_swap_backend_t *mock)
{
    const aied_swap_backend_t backend = {
        .load_journal = mock_load,
        .store_journal = mock_store,
        .swap_sector = mock_swap,
        .verify_pair = mock_verify,
        .commit_pair = mock_commit,
        .context = mock
    };
    return backend;
}
