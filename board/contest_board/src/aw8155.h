/****************************************************************************
 * vendor/openvela/boards/contest2026_148_board/src/aw8155.h
 *
 * AW8155 Audio PA on/off control via GPIO PA42.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 ****************************************************************************/

#ifndef __VENDOR_OPENVELA_BOARDS_CONTEST2026_148_BOARD_SRC_AW8155_H
#define __VENDOR_OPENVELA_BOARDS_CONTEST2026_148_BOARD_SRC_AW8155_H

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* Turn on the AW8155 PA (speaker amplifier).
 * Sends GPIO pulse sequence on PA42 and waits for PA to stabilize.
 *
 * mode: PA work mode 1-4, controls output power level.
 *       Use 1 for normal headset operation.
 */

void aw8155_pa_on(int mode);

/* Turn off the AW8155 PA (mute speaker). */

void aw8155_pa_off(void);

#endif /* __VENDOR_OPENVELA_BOARDS_CONTEST2026_148_BOARD_SRC_AW8155_H */
