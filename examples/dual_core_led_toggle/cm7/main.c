#include <stdbool.h>
#include <stdint.h>

#include "aied_board.h"
#include "app_events.h"
#include "app.h"
#include "board.h"
#include "fsl_common.h"
#include "fsl_rgpio.h"
#include "mcmgr.h"

#define APP_LED_MASK (1UL << AIED_BOARD_LED_PIN)

static volatile bool s_toggle_requested = false;

/**
 * @brief Configure the board user LED (aied_board.h); BOARD_InitPins() has already muxed the pin.
 */
static void app_led_init(void)
{
    rgpio_pin_config_t ledConfig = {
        .pinDirection = kRGPIO_DigitalOutput,
        .outputLogic = 1U,
    };

    RGPIO_PinInit(AIED_BOARD_LED_GPIO, AIED_BOARD_LED_PIN, &ledConfig);
}

/**
 * @brief Receive application events sent by the CM33 core.
 *
 * @param coreNum Source core identifier.
 * @param eventData Event payload.
 * @param context Optional callback context.
 */
static void app_remote_event_handler(mcmgr_core_t coreNum, uint16_t eventData, void *context)
{
    (void)context;

    if ((coreNum == kMCMGR_Core0) && (eventData == APP_EVENT_TOGGLE_LED)) {
        s_toggle_requested = true;
    }
}

/**
 * @brief CM7 secondary-core application entry point.
 *
 * @return This function does not return.
 */
int main(void)
{
    BOARD_InitHardware();
    app_led_init();

    (void)MCMGR_Init();

    /* Startup-data handshake: CM33 starts this core with kMCMGR_Start_Synchronous and waits for it. */
    uint32_t startup_data;
    while (MCMGR_GetStartupData(kMCMGR_Core0, &startup_data) != kStatus_MCMGR_Success) {
    }

    if (MCMGR_RegisterEvent(kMCMGR_RemoteApplicationEvent, app_remote_event_handler, NULL) != kStatus_MCMGR_Success) {
        for (;;) {
        }
    }

    if (MCMGR_TriggerEvent(kMCMGR_Core0, kMCMGR_RemoteApplicationEvent, APP_EVENT_CM7_READY) != kStatus_MCMGR_Success) {
        for (;;) {
        }
    }

    for (;;) {
        if (s_toggle_requested) {
            s_toggle_requested = false;
            RGPIO_PortToggle(AIED_BOARD_LED_GPIO, APP_LED_MASK);
        }
    }
}
