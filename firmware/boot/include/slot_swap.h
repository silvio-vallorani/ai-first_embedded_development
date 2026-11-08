#ifndef AIED_SLOT_SWAP_H
#define AIED_SLOT_SWAP_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    AIED_SWAP_IDLE = 0,
    AIED_SWAP_IN_PROGRESS = 1,
    AIED_SWAP_IMAGES_VERIFIED = 2,
    AIED_SWAP_COMMITTED = 3
} aied_swap_state_t;

typedef struct {
    aied_swap_state_t state;
    uint32_t cm33_sectors_total;
    uint32_t cm33_sectors_done;
    uint32_t cm7_sectors_total;
    uint32_t cm7_sectors_done;
    uint32_t generation;
} aied_swap_journal_t;

typedef struct {
    bool (*load_journal)(void *context, aied_swap_journal_t *journal);
    bool (*store_journal)(void *context, const aied_swap_journal_t *journal);
    bool (*swap_sector)(void *context, uint32_t core_id, uint32_t sector_index);
    bool (*verify_pair)(void *context);
    bool (*commit_pair)(void *context, uint32_t generation);
    void *context;
} aied_swap_backend_t;

/**
 * @brief Resume or execute a paired CM33/CM7 slot swap.
 * @note Journal writes must be durable and atomic. Each swap_sector operation must be
 *       restartable after power loss, using its own scratch-sector recovery protocol.
 *       commit_pair must be idempotent because reset may occur after commit but before
 *       the COMMITTED journal record is persisted.
 * @param backend Persistent journal and flash-operation callbacks.
 * @return true only when both images are swapped, verified and pair-committed.
 */
bool aied_slot_swap_execute(const aied_swap_backend_t *backend);

#endif
