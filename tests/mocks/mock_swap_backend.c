#include "mock_swap_backend.h"

#include <string.h>

static bool mock_load(void *context, aied_swap_journal_t *journal)
{
    mock_swap_backend_t *mock = context;
    *journal = mock->journal;
    return true;
}

static bool mock_store(void *context, const aied_swap_journal_t *journal)
{
    mock_swap_backend_t *mock = context;
    mock->journal = *journal;
    return true;
}

static bool mock_swap(void *context, uint32_t core_id, uint32_t sector_index)
{
    mock_swap_backend_t *mock = context;
    (void)sector_index;
    mock->operation_count++;
    if ((mock->fail_on_operation != 0U) &&
        (mock->operation_count == mock->fail_on_operation)) {
        return false;
    }
    if (core_id == 0U) {
        mock->swapped_cm33++;
    } else {
        mock->swapped_cm7++;
    }
    return true;
}

static bool mock_verify(void *context)
{
    return ((mock_swap_backend_t *)context)->verify_ok;
}

static bool mock_commit(void *context, uint32_t generation)
{
    mock_swap_backend_t *mock = context;
    (void)generation;
    if (mock->fail_commit) {
        return false;
    }
    mock->committed = true;
    return true;
}

void mock_swap_backend_init(mock_swap_backend_t *mock, uint32_t cm33_sectors, uint32_t cm7_sectors)
{
    (void)memset(mock, 0, sizeof(*mock));
    mock->journal.state = AIED_SWAP_IDLE;
    mock->journal.cm33_sectors_total = cm33_sectors;
    mock->journal.cm7_sectors_total = cm7_sectors;
    mock->verify_ok = true;
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
