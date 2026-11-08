#ifndef APP_EVENTS_H
#define APP_EVENTS_H

#include <stdint.h>

/** @brief Application event identifiers exchanged through MCMGR. */
typedef enum
{
    APP_EVENT_CM7_READY = 0x100U,
    APP_EVENT_TOGGLE_LED = 0x101U
} app_event_t;

#define APP_TOGGLE_PERIOD_US (1000000U)

#endif /* APP_EVENTS_H */
