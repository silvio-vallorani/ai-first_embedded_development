#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_events.h"
#include "app.h"
#include "board.h"
#include "fsl_common.h"
#include "mcmgr.h"

#define APP_CM7_READY_TIMEOUT_ITERATIONS (10000000UL)

#if !defined(APP_SECONDARY_CORE)
#define APP_SECONDARY_CORE kMCMGR_Core1
#endif

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

    (void)MCMGR_Init();
    BOARD_InitHardware();

    if (MCMGR_RegisterEvent(kMCMGR_RemoteApplicationEvent,
                            app_remote_event_handler,
                            NULL) != kStatus_MCMGR_Success)
    {
        for (;;)
        {
        }
    }

#ifdef CORE1_IMAGE_COPY_TO_RAM
    /*
     * Copy the secondary image using the linker symbols and memory address
     * provided by the NXP board-specific multicore project configuration.
     */
    {
        uint32_t core1_image_size = get_core1_image_size();

        (void)memcpy((void *)(char *)CORE1_BOOT_ADDRESS,
                     (const void *)CORE1_IMAGE_START,
                     core1_image_size);

#ifdef APP_INVALIDATE_CACHE_FOR_SECONDARY_CORE_IMAGE_MEMORY
        invalidate_cache_for_core1_image_memory((uint32_t)CORE1_BOOT_ADDRESS,
                                                core1_image_size);
#endif
    }
#endif

    if (MCMGR_StartCore(APP_SECONDARY_CORE,
                        (void *)(char *)CORE1_BOOT_ADDRESS,
                        2U,
                        kMCMGR_Start_Synchronous) != kStatus_MCMGR_Success)
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
                               APP_EVENT_TOGGLE_LED) != kStatus_MCMGR_Success)
        {
            for (;;)
            {
            }
        }
    }
}
