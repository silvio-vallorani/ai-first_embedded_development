#ifndef AIED_BOARD_H
#define AIED_BOARD_H

/* MIMXRT1180-EVK board definitions; pins are configured by the SDK board-port pin_mux.c. */

#define AIED_BOARD_NAME "MIMXRT1180-EVK"

/* User LED driven by the CM7 image: green LED on GPIO_AD_27 = GPIO4_IO27, active high. */
#define AIED_BOARD_LED_GPIO RGPIO4
#define AIED_BOARD_LED_PIN  (27U)

#endif
