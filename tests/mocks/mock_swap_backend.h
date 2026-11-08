#ifndef AIED_MOCK_SWAP_BACKEND_H
#define AIED_MOCK_SWAP_BACKEND_H

#include "slot_swap.h"

#define MOCK_SWAP_MAX_OPERATIONS 32U
typedef struct {
    aied_swap_journal_t journal;
    uint32_t operation_count;
    uint32_t fail_on_operation;
    uint32_t swapped_cm33;
    uint32_t swapped_cm7;
    bool verify_ok;
    bool committed;
    bool fail_commit;
} mock_swap_backend_t;

void mock_swap_backend_init(mock_swap_backend_t *mock, uint32_t cm33_sectors, uint32_t cm7_sectors);
aied_swap_backend_t mock_swap_backend_interface(mock_swap_backend_t *mock);

#endif
