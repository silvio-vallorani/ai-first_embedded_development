#ifndef AIED_BOARD_H
#define AIED_BOARD_H

/*
 * AM254-5 (AM Microsystems, EtherCAT load-cell converter, rev 5) board definitions.
 * Source: schematic AM254-5 rev 5, sheets MCU_1/MCU_2 (U9) and Main (DL1..DL5). See board.yml.
 */

#define AIED_BOARD_NAME "AM254-5"

/* User LED driven by the CM7 image: DL1 green, R95 1 kOhm, GPIO_EMC_B1_41 (ball R1) = GPIO3_IO09, active high. */
#define AIED_BOARD_LED_PINMUX IOMUXC_GPIO_EMC_B1_41_GPIO3_IO09
#define AIED_BOARD_LED_GPIO   RGPIO3
#define AIED_BOARD_LED_PIN    (9U)

#endif
