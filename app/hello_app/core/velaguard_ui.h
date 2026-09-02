/****************************************************************************
 * VelaGuard LVGL platform ownership.
 ****************************************************************************/

#ifndef __CONTEST2026_148_LANGYONGYUNJI_APP_HELLO_APP_VELAGUARD_UI_H
#define __CONTEST2026_148_LANGYONGYUNJI_APP_HELLO_APP_VELAGUARD_UI_H

#include <stdint.h>

int vg_ui_prepare(const char *lcd_path, const char *input_path,
                  unsigned int timeout_ms, unsigned int wait_step_ms);
int vg_ui_init(unsigned int input_poll_ms);
uint32_t vg_ui_process(void);

#endif
