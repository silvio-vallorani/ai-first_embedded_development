#ifndef AIED_AM254_4_CM7_PIN_MUX_H
#define AIED_AM254_4_CM7_PIN_MUX_H

/* AM254-4 replacement for the SDK board-port pin_mux.h of the CM7 image (same API). */

#if defined(__cplusplus)
extern "C" {
#endif

/** @brief Pins needed before main(); none on this board. */
void BOARD_InitBootPins(void);

/** @brief Pins owned by the CM7 image: the user LED only. */
void BOARD_InitPins(void);

#if defined(__cplusplus)
}
#endif

#endif
