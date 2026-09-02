/****************************************************************************
 * VelaGuard battery monitor task.
 ****************************************************************************/

#ifndef __CONTEST2026_148_LANGYONGYUNJI_APP_HELLO_APP_VELAGUARD_BATTERY_H
#define __CONTEST2026_148_LANGYONGYUNJI_APP_HELLO_APP_VELAGUARD_BATTERY_H

#include <stdbool.h>
#include <stdint.h>

struct vg_battery_status_s
{
  int millivolts;
  int percentage;
  int last_error;
  uint32_t sequence;
  bool ready;
  bool charging;
};

int vg_battery_task_start(int priority, int stacksize);
void vg_battery_task_get_status(struct vg_battery_status_s *status);

#endif
