/*
 * AM254-4 pin configuration of the CM7 image.
 *
 * Replaces the EVK board-port pin_mux.c, which configures GPIO_AD_27 as the user LED:
 * on the AM254-4 GPIO_AD_27 is ADC_MCLK of the AD7767 (U11) and must not be touched here.
 * The UART pins belong to the CM33 image (debug console) and are not configured by CM7.
 */

#include "fsl_common.h"
#include "fsl_iomuxc.h"
#include "pin_mux.h"
#include "aied_board.h"

void BOARD_InitBootPins(void)
{
}

void BOARD_InitPins(void)
{
    CLOCK_EnableClock(kCLOCK_Iomuxc1);
    CLOCK_EnableClock(kCLOCK_Iomuxc2);

    /* DL1 (green) through R95 3.3 kOhm to GND: GPIO_EMC_B1_41 as GPIO3_IO09, active high. */
    IOMUXC_SetPinMux(AIED_BOARD_LED_PINMUX, 0U);
}
