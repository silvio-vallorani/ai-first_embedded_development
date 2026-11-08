#ifndef APP_EVENTS_H
#define APP_EVENTS_H

#include <stdint.h>

/** @brief Application event identifiers exchanged through MCMGR. */
typedef enum
{
    APP_EVENT_CM7_READY = 0x100U,
    APP_EVENT_TOGGLE_LED = 0x101U
} app_event_t;

/** @brief LED toggle period; the build may override it to tell update-test images apart. */
#ifndef APP_TOGGLE_PERIOD_US
#define APP_TOGGLE_PERIOD_US (1000000U)
#endif

/**
 * @brief Application version string.
 *
 * The build passes the signed image version as numeric APP_VERSION_MAJOR/MINOR/PATCH
 * macros (no quoted string on the compiler command line).
 */
#if defined(APP_VERSION_MAJOR) && defined(APP_VERSION_MINOR) && defined(APP_VERSION_PATCH)
#define APP_STRINGIFY_(x) #x
#define APP_STRINGIFY(x) APP_STRINGIFY_(x)
#define APP_VERSION_STRING \
    APP_STRINGIFY(APP_VERSION_MAJOR) "." APP_STRINGIFY(APP_VERSION_MINOR) "." APP_STRINGIFY(APP_VERSION_PATCH)
#else
#define APP_VERSION_STRING "dev"
#endif

#endif /* APP_EVENTS_H */
