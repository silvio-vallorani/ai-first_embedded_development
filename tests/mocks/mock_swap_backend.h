#ifndef AIED_MOCK_SWAP_BACKEND_H
#define AIED_MOCK_SWAP_BACKEND_H

#include "slot_swap.h"

#define MOCK_SWAP_MAX_SECTORS 8U

/**
 * @brief Fake swap backend with per-sector slot state and power-cut injection.
 *
 * Side-effecting operations (store, swap, commit) are numbered from 1. When the operation
 * number equals cut_at, power is lost: that and every following operation fails without
 * effect until mock_swap_backend_reboot() is called.
 */
typedef struct {
    aied_swap_journal_t journal;
    bool journal_corrupt;                          /**< load_journal fails. */
    bool sector_swapped[2][MOCK_SWAP_MAX_SECTORS]; /**< true: candidate content is active. */
    uint32_t sectors[2];                           /**< Sectors per core (CM33, CM7). */
    bool candidate_corrupt;                        /**< Candidate pair fails verification. */
    bool previous_corrupt;                         /**< Previous pair fails verification. */
    uint32_t operation_count;
    uint32_t cut_at;
    bool power_lost;
    uint32_t commit_count;
} mock_swap_backend_t;

/**
 * @brief Reset the fake to a blank journal with the given image sizes.
 * @param mock Fake instance.
 * @param cm33_sectors CM33 sector count (at most MOCK_SWAP_MAX_SECTORS).
 * @param cm7_sectors CM7 sector count (at most MOCK_SWAP_MAX_SECTORS).
 */
void mock_swap_backend_init(mock_swap_backend_t *mock, uint32_t cm33_sectors, uint32_t cm7_sectors);

/**
 * @brief Restore power after an injected cut, keeping all persisted state.
 * @param mock Fake instance.
 */
void mock_swap_backend_reboot(mock_swap_backend_t *mock);

/**
 * @brief Return true when every sector of both cores holds the given arrangement.
 * @param mock Fake instance.
 * @param swapped Expected arrangement (true: candidate active).
 * @return true when the active pair is entirely in that arrangement.
 */
bool mock_swap_backend_all_sectors(const mock_swap_backend_t *mock, bool swapped);

/**
 * @brief Build the backend interface bound to the fake.
 * @param mock Fake instance.
 * @return Backend callbacks.
 */
aied_swap_backend_t mock_swap_backend_interface(mock_swap_backend_t *mock);

#endif
