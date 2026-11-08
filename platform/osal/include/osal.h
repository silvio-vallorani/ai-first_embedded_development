#ifndef AIED_OSAL_H
#define AIED_OSAL_H

#include <stdbool.h>
#include <stdint.h>

typedef uint32_t osal_tick_t;
typedef void (*osal_task_entry_t)(void *argument);

/**
 * @brief Return the monotonic OSAL tick count.
 * @return Current tick count.
 */
osal_tick_t osal_get_ticks(void);

/**
 * @brief Request a scheduler-independent delay.
 * @param ticks Number of OSAL ticks to wait.
 */
void osal_delay(osal_tick_t ticks);

#endif
