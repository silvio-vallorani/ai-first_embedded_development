#ifndef AIED_SLOT_SWAP_H
#define AIED_SLOT_SWAP_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Pair-level update transaction states persisted in the swap journal.
 *
 * IDLE, COMMITTED and REVERTED are terminal: the active pair is a confirmed pair.
 * Every other state belongs to an unfinished transaction.
 */
typedef enum {
    AIED_SWAP_IDLE = 0,       /**< No transaction recorded. */
    AIED_SWAP_PREPARED = 1,   /**< Validated candidate pair staged in the inactive slot. */
    AIED_SWAP_SWAPPING = 2,   /**< Forward sector swap in progress. */
    AIED_SWAP_TRIAL = 3,      /**< New pair swapped and verified; running on probation. */
    AIED_SWAP_COMMITTING = 4, /**< Confirmation recorded; pair commit in progress. */
    AIED_SWAP_COMMITTED = 5,  /**< New pair confirmed. */
    AIED_SWAP_REVERTING = 6,  /**< Reverse sector swap restoring the previous pair. */
    AIED_SWAP_REVERTED = 7    /**< Previous pair restored; the candidate was rejected. */
} aied_swap_state_t;

/** @brief Direction of a sector swap operation. */
typedef enum {
    AIED_SWAP_FORWARD = 0, /**< Move the candidate pair to the active addresses. */
    AIED_SWAP_REVERT = 1   /**< Move the previous pair back to the active addresses. */
} aied_swap_direction_t;

/** @brief Action the boot manager must take after evaluating the journal. */
typedef enum {
    AIED_BOOT_NORMAL = 0,  /**< Start the confirmed active pair. */
    AIED_BOOT_TRIAL = 1,   /**< Start the active pair on probation; confirmation required. */
    AIED_BOOT_RECOVERY = 2 /**< No runnable pair can be guaranteed; enter recovery. */
} aied_boot_action_t;

/** @brief Persistent pair-level swap journal record (fixed-width fields only). */
typedef struct {
    uint32_t state;              /**< One of aied_swap_state_t. */
    uint32_t cm33_sectors_total; /**< CM33 sectors to swap. */
    uint32_t cm33_sectors_done;  /**< CM33 sectors swapped in the current direction. */
    uint32_t cm7_sectors_total;  /**< CM7 sectors to swap. */
    uint32_t cm7_sectors_done;   /**< CM7 sectors swapped in the current direction. */
    uint32_t trial_boots;        /**< Probationary boots started without confirmation. */
    uint32_t generation;         /**< Incremented on every journal write. */
} aied_swap_journal_t;

/**
 * @brief Persistent journal and flash-operation callbacks.
 *
 * Contract:
 * - load_journal returns the newest intact record, or an IDLE record on a blank journal.
 *   It returns false only when no intact record can be found (integrity check failure).
 *   Record integrity (CRC, redundancy) is owned by the backend.
 * - store_journal writes are durable and atomic.
 * - swap_sector exchanges one sector between the active and inactive slot of a core
 *   (core_id 0 = CM33, 1 = CM7). It must be restartable after power loss and idempotent
 *   for a given (direction, core_id, sector_index): if the sector is already in the target
 *   arrangement for that direction, it succeeds without changing it.
 * - verify_pair checks the pair currently at the fixed active addresses.
 * - commit_pair makes the pair confirmation permanent (e.g. advances the anti-rollback
 *   counter). It must be idempotent: it is repeated if reset occurs before COMMITTED
 *   is persisted.
 */
typedef struct {
    bool (*load_journal)(void *context, aied_swap_journal_t *journal);
    bool (*store_journal)(void *context, const aied_swap_journal_t *journal);
    bool (*swap_sector)(void *context, uint32_t core_id, uint32_t sector_index,
                        aied_swap_direction_t direction);
    bool (*verify_pair)(void *context);
    bool (*commit_pair)(void *context, uint32_t generation);
    void *context;
} aied_swap_backend_t;

/**
 * @brief Stage a validated candidate pair for activation.
 * @note Call only after the release manifest and both images in the inactive slot have
 *       been validated. Allowed only when no transaction is in progress.
 * @param backend Persistent journal and flash-operation callbacks.
 * @param cm33_sectors Number of CM33 sectors to swap (non-zero).
 * @param cm7_sectors Number of CM7 sectors to swap (non-zero).
 * @return true when the PREPARED record is persisted.
 */
bool aied_slot_swap_prepare(const aied_swap_backend_t *backend,
                            uint32_t cm33_sectors,
                            uint32_t cm7_sectors);

/**
 * @brief Evaluate and advance the update transaction at boot, before any core starts.
 *
 * Resumes an interrupted swap, revert or commit; starts the probation of a newly swapped
 * pair; and reverts the pair as one unit when verification fails or the trial budget is
 * exhausted.
 * @param backend Persistent journal and flash-operation callbacks.
 * @param max_trial_boots Unconfirmed probationary boots allowed before revert (non-zero).
 * @return The action the boot manager must take. Neither core may start on RECOVERY.
 */
aied_boot_action_t aied_slot_swap_boot(const aied_swap_backend_t *backend,
                                       uint32_t max_trial_boots);

/**
 * @brief Confirm the pair running on probation after both cores passed health checks.
 * @param backend Persistent journal and flash-operation callbacks.
 * @return true when the pair is committed (also when it was already committed).
 */
bool aied_slot_swap_confirm(const aied_swap_backend_t *backend);

#endif
