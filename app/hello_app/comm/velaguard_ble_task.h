/****************************************************************************
 * VelaGuard BLE task.
 *
 * The main task owns one-time Bluetooth framework initialization and submits
 * application requests.  This task owns the periodic BLE state machine,
 * including advertising maintenance and outgoing notifications.
 ****************************************************************************/

#ifndef __VELAGUARD_BLE_TASK_H
#define __VELAGUARD_BLE_TASK_H

#include <stdbool.h>

struct vg_ble_call_packet_s;

/* Create the periodic BLE worker.  The function is idempotent. */
int vg_ble_task_start(void);
int vg_ble_task_send_enable(bool enabled);
int vg_ble_task_send_call(const struct vg_ble_call_packet_s *packet);
bool vg_ble_task_has_pending_enable(void);

#endif /* __VELAGUARD_BLE_TASK_H */
