#include <stdbool.h>
#include <stdint.h>

#include "app_events.h"
#include "fsl_common.h"
#include "mcmgr.h"

/*
 * The CM7 image address is a board/linker integration parameter.
 * Define APP_CM7_BOOT_ADDRESS from the CM7 image load/entry address generated
 * by the selected MCUXpresso SDK MIMXRT1180-EVK configuration.
 */
#ifndef APP_CM7_BOOT_ADDRESS
#error "APP_CM7_BOOT_ADDRESS must be defined by the CM7 image build configuration"
#endif

#define APP_CM7_READY_TIMEOUT_ITERATIONS (10000000UL)

static volatile bool s_cm7_ready = false;

/**
 * @brief Receive application events sent by the CM7 core.
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

    if ((coreNum == kMCMGR_Core1) && (eventData == APP_EVENT_CM7_READY))
    {
        s_cm7_ready = true;
    }
}

/**
 * @brief CM33 primary-core application entry point.
 *
 * @return This function does not return.
 */
int main(void)
{
    uint32_t iteration = 0U;

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

    /*
     * The CM7 image is linked into the primary multicore image by the SDK
     * project. The address is supplied by that project's linker integration.
     */
    if (MCMGR_StartCore(kMCMGR_Core1,
                        (uint32_t)APP_CM7_BOOT_ADDRESS,
                        0U,
                        kMCMGR_Start_Synchronous) != kStatus_Success)
    {
        for (;;)
        {
        }
    }

    while (!s_cm7_ready)
    {
        if (++iteration >= APP_CM7_READY_TIMEOUT_ITERATIONS)
        {
            for (;;)
            {
            }
        }
    }

    for (;;)
    {
        SDK_DelayAtLeastUs(APP_TOGGLE_PERIOD_US, SystemCoreClock);

        if (MCMGR_TriggerEvent(kMCMGR_Core1,
                               kMCMGR_RemoteApplicationEvent,
                               APP_EVENT_TOGGLE_LED) != kStatus_Success)
        {
            for (;;)
            {
            }
        }
    }
}
