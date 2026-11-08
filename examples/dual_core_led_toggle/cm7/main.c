#include <stdbool.h>
#include <stdint.h>

#include "app_events.h"
#include "fsl_clock.h"
#include "fsl_common.h"
#include "fsl_gpio.h"
#include "fsl_iomuxc.h"
#include "mcmgr.h"

#define APP_LED_GPIO GPIO4
#define APP_LED_GPIO_PIN (27U)
#define APP_LED_MASK (1UL << APP_LED_GPIO_PIN)

static volatile bool s_toggle_requested = false;

/**
 * @brief Configure the EVK green user LED on GPIO_AD_27 / GPIO4_IO27.
 */
static void app_led_init(void)
{
    gpio_pin_config_t ledConfig = {
        .direction = kGPIO_DigitalOutput,
        .outputLogic = 1U,
        .interruptMode = kGPIO_NoIntmode,
    };

    CLOCK_EnableClock(kCLOCK_Gpio4);
    IOMUXC_SetPinMux(IOMUXC_GPIO_AD_27_GPIO4_IO27, 0U);
    GPIO_PinInit(APP_LED_GPIO, APP_LED_GPIO_PIN, &ledConfig);
}

/**
 * @brief Receive application events sent by the CM33 core.
 *
 * @param coreNum Source core identifier.
 * @param eventData Event payload.
 * @param context Optional callback context.
 */
static void app_remote_event_handler(mcmgr_core_t coreNum,
                                     uint16_t eventData,
                                     void *context)
{
    (void)context;

    if ((coreNum == kMCMGR_Core0) && (eventData == APP_EVENT_TOGGLE_LED))
    {
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
    app_led_init();

    MCMGR_EarlyInit();
    MCMGR_Init();

    if (MCMGR_RegisterEvent(kMCMGR_RemoteApplicationEvent,
                            app_remote_event_handler,
                            NULL) != kStatus_Success)
    {
        for (;;)
        {
        }
    }

    if (MCMGR_TriggerEvent(kMCMGR_Core0,
                           kMCMGR_RemoteApplicationEvent,
                           APP_EVENT_CM7_READY) != kStatus_Success)
    {
        for (;;)
        {
        }
    }

    for (;;)
    {
        if (s_toggle_requested)
        {
            s_toggle_requested = false;
            GPIO_PortToggle(APP_LED_GPIO, APP_LED_MASK);
        }
    }
}
