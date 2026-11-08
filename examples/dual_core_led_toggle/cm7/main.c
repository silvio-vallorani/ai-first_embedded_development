#include <stdbool.h>
#include <stdint.h>

#include "app_events.h"
#include "app.h"
#include "board.h"
#include "fsl_common.h"
#include "fsl_rgpio.h"
#include "fsl_iomuxc.h"
#include "mcmgr.h"

#define APP_LED_GPIO     RGPIO4
#define APP_LED_GPIO_PIN (27U)
#define APP_LED_MASK     (1UL << APP_LED_GPIO_PIN)

static volatile bool s_toggle_requested = false;

/**
 * @brief Configure the EVK green user LED on GPIO_AD_27 / GPIO4_IO27.
 */
static void app_led_init(void)
{
    rgpio_pin_config_t ledConfig = {
        .pinDirection = kRGPIO_DigitalOutput,
        .outputLogic = 1U,
    };

    IOMUXC_SetPinMux(IOMUXC_GPIO_AD_27_GPIO4_IO27, 0U);
    RGPIO_PinInit(APP_LED_GPIO, APP_LED_GPIO_PIN, &ledConfig);
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
            RGPIO_PortToggle(APP_LED_GPIO, APP_LED_MASK);
        }
    }
}
