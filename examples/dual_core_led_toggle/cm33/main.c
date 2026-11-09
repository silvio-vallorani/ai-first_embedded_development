#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "aied_log.h"
#include "aied_log_port.h"
#include "app_events.h"
#include "app.h"
#include "board.h"
#include "fsl_common.h"
#include "mcmgr.h"

#if defined(AIED_MCUBOOT_APP)
#include "mcuboot_app_support.h"
#include "mflash_drv.h"
#endif

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define APP_CM7_READY_TIMEOUT_US (2000000ULL)

#if !defined(APP_SECONDARY_CORE)
#define APP_SECONDARY_CORE kMCMGR_Core1
#endif

static volatile bool s_cm7_ready = false;

/**
 * @brief Stop the application after a fatal error, keeping the log alive.
 *
 * The error record is written to flash and the UART `logdump` command keeps working.
 */
static void app_halt(void)
{
    for (;;) {
        aied_log_port_poll();
    }
}

#if defined(AIED_MCUBOOT_APP)
/**
 * @brief Make a probationary (test) image permanent after the pair health check.
 *
 * Called only after CM7 has started and reported ready, so the confirmation covers the
 * whole CM33 + CM7 release pair. If the image is not confirmed, MCUboot reverts the pair
 * at the next reset.
 */
static void app_confirm_image(void)
{
    uint32_t state = kSwapType_None;

    if ((mflash_drv_init() != 0) || (bl_get_image_state(0U, &state) != kStatus_Success)) {
        LOG_SEC_ERR("MCUboot: unable to read image state");
        return;
    }
#if defined(AIED_SKIP_IMAGE_CONFIRM)
    LOG_SEC_WRN("MCUboot: image %s state %u, confirmation disabled in this build", APP_VERSION_STRING, (unsigned)state);
#else
    if (state == kSwapType_Testing) {
        if (bl_update_image_state(0U, kSwapType_Permanent) == kStatus_Success) {
            LOG_SEC_INF("MCUboot: test image %s confirmed (permanent)", APP_VERSION_STRING);
        } else {
            LOG_SEC_ERR("MCUboot: image %s confirmation FAILED", APP_VERSION_STRING);
        }
    } else {
        LOG_SEC_INF("MCUboot: image %s state %u, nothing to confirm", APP_VERSION_STRING, (unsigned)state);
    }
#endif
}
#endif

/**
 * @brief Receive application events sent by the CM7 core.
 *
 * @param coreNum Source core identifier.
 * @param eventData Event payload.
 * @param context Optional callback context.
 */
static void app_remote_event_handler(mcmgr_core_t coreNum, uint16_t eventData, void *context)
{
    (void)context;

    if ((coreNum == kMCMGR_Core1) && (eventData == APP_EVENT_CM7_READY)) {
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
    uint64_t deadline;

    (void)MCMGR_Init();
    BOARD_InitHardware();
    (void)aied_log_port_init(APP_VERSION_STRING);

    LOG_INF("AIED dual-core LED toggle %s (toggle period %u us)", APP_VERSION_STRING, (unsigned)APP_TOGGLE_PERIOD_US);

    if (MCMGR_RegisterEvent(kMCMGR_RemoteApplicationEvent, app_remote_event_handler, NULL) != kStatus_MCMGR_Success) {
        LOG_ERR("MCMGR event registration failed");
        app_halt();
    }

#ifdef CORE1_IMAGE_COPY_TO_RAM
    /*
     * Copy the secondary image using the linker symbols and memory address
     * provided by the NXP board-specific multicore project configuration.
     */
    {
        uint32_t core1_image_size = get_core1_image_size();

        (void)memcpy((void *)(char *)CORE1_BOOT_ADDRESS, (const void *)CORE1_IMAGE_START, core1_image_size);

#ifdef APP_INVALIDATE_CACHE_FOR_SECONDARY_CORE_IMAGE_MEMORY
        invalidate_cache_for_core1_image_memory((uint32_t)CORE1_BOOT_ADDRESS, core1_image_size);
#endif
    }
#endif

    if (MCMGR_StartCore(APP_SECONDARY_CORE, (void *)(char *)CORE1_BOOT_ADDRESS, 2U, kMCMGR_Start_Synchronous) !=
        kStatus_MCMGR_Success) {
        LOG_ERR("CM7 start failed");
        app_halt();
    }

    deadline = aied_log_port_now_us() + APP_CM7_READY_TIMEOUT_US;
    while (!s_cm7_ready) {
        aied_log_port_poll();
        if (aied_log_port_now_us() >= deadline) {
            LOG_ERR("CM7 ready timeout");
            app_halt();
        }
    }
    LOG_INF("CM7 ready");

#if defined(AIED_MCUBOOT_APP)
    app_confirm_image();
#endif

    /* Non-blocking loop: logs are flushed and the UART command is served between toggles. */
    deadline = aied_log_port_now_us() + APP_TOGGLE_PERIOD_US;
    for (;;) {
        uint64_t now;

        aied_log_port_poll();
        now = aied_log_port_now_us();
        if (now >= deadline) {
            /* After a long flash operation, realign instead of toggling in a burst. */
            deadline = ((now - deadline) >= APP_TOGGLE_PERIOD_US) ? (now + APP_TOGGLE_PERIOD_US)
                                                                  : (deadline + APP_TOGGLE_PERIOD_US);
            if (MCMGR_TriggerEvent(kMCMGR_Core1, kMCMGR_RemoteApplicationEvent, APP_EVENT_TOGGLE_LED) !=
                kStatus_MCMGR_Success) {
                LOG_ERR("LED toggle event failed");
                app_halt();
            }
        }
    }
}
