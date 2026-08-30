/****************************************************************************
 * VelaGuard BLE emergency-call transport.
 ****************************************************************************/

#ifndef __VELAGUARD_BLE_H
#define __VELAGUARD_BLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum vg_ble_event_type_e
{
  VG_BLE_EVENT_MANUAL_SOS = 1,
  VG_BLE_EVENT_FALL       = 2,
  VG_BLE_EVENT_VOICE_SOS  = 3,
};

/* The phone receives this packed 16-byte packet from the Event
 * characteristic. All multi-byte values are little-endian.
 */

struct vg_ble_call_packet_s
{
  uint8_t magic[2];       /* "VG" */
  uint8_t version;        /* Protocol version, currently 1 */
  uint8_t command;        /* 1 = CALL_REQUEST */
  uint8_t event_type;     /* enum vg_ble_event_type_e */
  uint8_t risk;
  uint8_t confidence;
  uint8_t flags;          /* bit 0 = user confirmed */
  uint32_t event_id;
  uint32_t uptime_ms;
} __attribute__((packed));

int vg_ble_init(void);
void vg_ble_process(void);
void vg_ble_process_ui(void);
void vg_ble_process_time(void);
void vg_ble_set_fall_status(bool active);
bool vg_ble_is_connected(void);
bool vg_ble_is_initialized(void);
bool vg_ble_is_enabled(void);
bool vg_ble_is_ready(void);
bool vg_ble_is_advertising(void);
bool vg_ble_has_pending_enable_request(void);
int vg_ble_set_enabled(bool enabled);
void vg_ble_request_set_enabled(bool enabled);
void vg_ble_get_local_address(char *buf, size_t len);
void vg_ble_get_device_name(char *buf, size_t len);
bool vg_ble_local_address_is_default(void);
int vg_ble_request_call(uint8_t event_type, uint8_t risk,
                        uint8_t confidence, uint32_t event_id,
                        uint32_t uptime_ms, bool user_confirmed);

#endif
