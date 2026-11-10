#ifndef AIED_BOARD_H
#define AIED_BOARD_H

/*
 * AM254-4 (AM Microsystems, EtherCAT load-cell converter) board definitions.
 * Source: schematic AM254-4 (title block rev 3), sheets MCU_1/MCU_2 (U9) and Main (DL1..DL5). See board.yml.
 */

#define AIED_BOARD_NAME "AM254-4"

/* User LED driven by the CM7 image: DL1 green, R95 3.3 kOhm, GPIO_EMC_B1_41 (ball R1) = GPIO3_IO09, active high. */
#define AIED_BOARD_LED_PINMUX IOMUXC_GPIO_EMC_B1_41_GPIO3_IO09
#define AIED_BOARD_LED_GPIO   RGPIO3
#define AIED_BOARD_LED_PIN    (9U)

#endif
