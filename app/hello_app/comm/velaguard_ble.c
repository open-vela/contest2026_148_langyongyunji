/****************************************************************************
 * VelaGuard BLE emergency-call transport.
 ****************************************************************************/

#include <nuttx/config.h>

#include "velaguard_ble.h"

#include <errno.h>
#include <nuttx/irq.h>
#include <nuttx/spinlock.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "advertiser_data.h"
#include "bluetooth.h"
#include "bt_adapter.h"
#include "bt_gatts.h"
#include "bt_le_advertiser.h"

#define VG_BLE_NAME                 "VelaGuard"
#define VG_BLE_COMMAND_CALL_REQUEST 1
#define VG_BLE_FLAG_USER_CONFIRMED  1
#define VG_BLE_ADDR_TEXT_LEN        18
#define VG_BLE_TEST_NOTIFY_MS       1000
#define VG_BLE_ADV_RETRY_SKIP       250

#define VG_BLE_UUID_BYTES(id) \
  (id), 0x00, 0x47, 0x56, 0x64, 0x72, 0x61, 0x93, \
  0x61, 0x4c, 0x65, 0x76, 0x6e, 0x65, 0x70, 0x6f

enum vg_ble_handle_e
{
  VG_BLE_HANDLE_SERVICE = 1,
  VG_BLE_HANDLE_EVENT,
  VG_BLE_HANDLE_EVENT_CCC,
  VG_BLE_HANDLE_TIME,
  VG_BLE_HANDLE_TEST,
  VG_BLE_HANDLE_TEST_CCC,
};

static const bt_uuid_t g_vg_service_uuid =
  BT_UUID_DECLARE_128(VG_BLE_UUID_BYTES(0x01));
static const bt_uuid_t g_vg_event_uuid =
  BT_UUID_DECLARE_128(VG_BLE_UUID_BYTES(0x02));
static const bt_uuid_t g_vg_time_uuid =
  BT_UUID_DECLARE_128(VG_BLE_UUID_BYTES(0x03));
static const bt_uuid_t g_vg_test_uuid =
  BT_UUID_DECLARE_128(VG_BLE_UUID_BYTES(0x04));

static struct vg_ble_call_packet_s g_vg_last_packet;
static bt_instance_t *g_vg_bt;
static gatts_handle_t g_vg_gatts;
static bt_advertiser_t *g_vg_advertiser;
static bt_address_t g_vg_peer_addr;
static bool g_vg_peer_addr_valid;
static bool g_vg_connected;
static bool g_vg_notify_enabled;
static bool g_vg_initialized;
static bool g_vg_enabled;
static bool g_vg_adapter_ready;
static bool g_vg_service_registered;
static bool g_vg_attr_table_added;
static bool g_vg_advertising;
static bool g_vg_adv_starting;
static bool g_vg_call_pending;
static bool g_vg_local_addr_default;
static volatile bool g_vg_test_notify_enabled;
static volatile bool g_vg_test_probe_seen;
static volatile bool g_vg_enable_request_pending;
static volatile bool g_vg_enable_request_target;
static volatile bool g_vg_start_advertising_pending;
static volatile bool g_vg_time_sync_pending;
static unsigned int g_vg_adv_retry_skip;
static char g_vg_local_addr[VG_BLE_ADDR_TEXT_LEN];
static uint64_t g_vg_last_time_sync_ms;
static uint64_t g_vg_pending_time_sync_ms;
static uint64_t g_vg_last_test_notify_ms;
static uint32_t g_vg_test_counter;
static uint8_t g_vg_last_test_value;
static uint16_t g_vg_event_ccc_value;
static uint16_t g_vg_test_ccc_value;

static uint64_t vg_ble_get_le64(const uint8_t *data)
{
  uint64_t value = 0;
  int i;

  for (i = 7; i >= 0; i--)
    {
      value = (value << 8) | data[i];
    }

  return value;
}

static void vg_ble_put_le16(uint8_t *data, uint16_t value)
{
  data[0] = value & 0xff;
  data[1] = (value >> 8) & 0xff;
}

static uint16_t vg_ble_get_le16(const uint8_t *data)
{
  return data[0] | (data[1] << 8);
}

static void vg_ble_put_le32(uint8_t *data, uint32_t value)
{
  data[0] = value & 0xff;
  data[1] = (value >> 8) & 0xff;
  data[2] = (value >> 16) & 0xff;
  data[3] = (value >> 24) & 0xff;
}

static uint64_t vg_ble_monotonic_ms(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int vg_ble_status_to_errno(bt_status_t status)
{
  if (status == BT_STATUS_SUCCESS || status == BT_STATUS_DONE)
    {
      return 0;
    }

  if (status == BT_STATUS_BUSY)
    {
      return -EBUSY;
    }

  if (status == BT_STATUS_NOT_READY || status == BT_STATUS_NOT_ENABLED)
    {
      return -ENODEV;
    }

  if (status == BT_STATUS_NOMEM || status == BT_STATUS_NO_RESOURCES)
    {
      return -ENOMEM;
    }

  return -EIO;
}

static bool vg_ble_framework_ready(void)
{
  bt_adapter_state_t state;

  if (g_vg_bt == NULL)
    {
      return false;
    }

  state = bt_adapter_get_state(g_vg_bt);
  return state == BT_ADAPTER_STATE_ON || state == BT_ADAPTER_STATE_BLE_ON;
}

static void vg_ble_cache_local_address(void)
{
  bt_address_t addr;

  strlcpy(g_vg_local_addr, "pending", sizeof(g_vg_local_addr));
  g_vg_local_addr_default = false;

  if (g_vg_bt == NULL)
    {
      return;
    }

  memset(&addr, 0, sizeof(addr));
  bt_adapter_get_address(g_vg_bt, &addr);
  if (bt_addr_is_empty(&addr) || bt_addr_ba2str(&addr, g_vg_local_addr) < 0)
    {
      printf("VelaGuard BLE: local adapter address unavailable\n");
      strlcpy(g_vg_local_addr, "pending", sizeof(g_vg_local_addr));
      return;
    }

  g_vg_local_addr_default =
    strcmp(g_vg_local_addr, "CD:AB:78:56:34:12") == 0 ||
    strcmp(g_vg_local_addr, "12:34:56:78:AB:CD") == 0;
  printf("VelaGuard BLE: local adapter address %s%s\n", g_vg_local_addr,
         g_vg_local_addr_default ? " (default/test)" : "");
}

static uint16_t vg_ble_attr_read(gatts_handle_t srv_handle,
                                 bt_address_t *addr, uint16_t attr_handle,
                                 uint32_t req_handle)
{
  uint8_t ccc_value[2];
  uint8_t *data = NULL;
  uint16_t len = 0;

  switch (attr_handle)
    {
      case VG_BLE_HANDLE_EVENT:
        data = (uint8_t *)&g_vg_last_packet;
        len = sizeof(g_vg_last_packet);
        break;

      case VG_BLE_HANDLE_TIME:
        data = (uint8_t *)&g_vg_last_time_sync_ms;
        len = sizeof(g_vg_last_time_sync_ms);
        break;

      case VG_BLE_HANDLE_TEST:
        data = &g_vg_last_test_value;
        len = sizeof(g_vg_last_test_value);
        break;

      case VG_BLE_HANDLE_EVENT_CCC:
        vg_ble_put_le16(ccc_value, g_vg_event_ccc_value);
        data = ccc_value;
        len = sizeof(ccc_value);
        break;

      case VG_BLE_HANDLE_TEST_CCC:
        vg_ble_put_le16(ccc_value, g_vg_test_ccc_value);
        data = ccc_value;
        len = sizeof(ccc_value);
        break;

      default:
        printf("VelaGuard BLE: read unknown handle=0x%04x\n", attr_handle);
        return 0;
    }

  bt_gatts_response(srv_handle, addr, req_handle, data, len);
  return 0;
}

static uint16_t vg_ble_attr_write(gatts_handle_t srv_handle,
                                  bt_address_t *addr, uint16_t attr_handle,
                                  const uint8_t *value, uint16_t length,
                                  uint16_t offset)
{
  uint64_t epoch_ms;
  irqstate_t irqstate;
  uint16_t ccc_value;

  (void)srv_handle;
  (void)addr;

  if (offset != 0)
    {
      printf("VelaGuard BLE: write offset unsupported handle=0x%04x offset=%u\n",
             attr_handle, offset);
      return 0;
    }

  switch (attr_handle)
    {
      case VG_BLE_HANDLE_TIME:
        if (length != sizeof(epoch_ms))
          {
            printf("VelaGuard BLE: time sync invalid len=%u\n", length);
            return 0;
          }

        epoch_ms = vg_ble_get_le64(value);
        irqstate = enter_critical_section();
        g_vg_pending_time_sync_ms = epoch_ms;
        g_vg_time_sync_pending = true;
        leave_critical_section(irqstate);
        printf("VelaGuard BLE: time sync queued epoch_ms=%llu\n",
               (unsigned long long)epoch_ms);
        return length;

      case VG_BLE_HANDLE_TEST:
        if (length < 1)
          {
            return 0;
          }

        g_vg_last_test_value = value[0];
        g_vg_test_probe_seen = true;
        g_vg_last_test_notify_ms = 0;
        printf("VelaGuard BLE TEST: write len=%u first=0x%02x\n",
               length, g_vg_last_test_value);
        return length;

      case VG_BLE_HANDLE_EVENT_CCC:
      case VG_BLE_HANDLE_TEST_CCC:
        if (length < sizeof(ccc_value))
          {
            return 0;
          }

        ccc_value = vg_ble_get_le16(value);
        if (attr_handle == VG_BLE_HANDLE_EVENT_CCC)
          {
            g_vg_event_ccc_value = ccc_value;
            g_vg_notify_enabled = ccc_value == GATT_CCC_NOTIFY;
            printf("VelaGuard BLE: event CCC value=0x%04x notifications=%s\n",
                   ccc_value, g_vg_notify_enabled ? "enabled" : "disabled");
          }
        else
          {
            g_vg_test_ccc_value = ccc_value;
            g_vg_test_notify_enabled = ccc_value == GATT_CCC_NOTIFY;
            if (g_vg_test_notify_enabled)
              {
                g_vg_last_test_notify_ms = 0;
              }

            printf("VelaGuard BLE TEST: CCC value=0x%04x notifications=%s\n",
                   ccc_value,
                   g_vg_test_notify_enabled ? "enabled" : "disabled");
          }

        return length;

      default:
        printf("VelaGuard BLE: write unknown handle=0x%04x len=%u\n",
               attr_handle, length);
        return 0;
    }
}

static gatt_attr_db_t g_vg_attrs[] =
{
  GATT_H_PRIMARY_SERVICE(g_vg_service_uuid, VG_BLE_HANDLE_SERVICE),
  GATT_H_CHARACTERISTIC_USER_RSP(g_vg_event_uuid,
                                 GATT_PROP_READ | GATT_PROP_NOTIFY,
                                 GATT_PERM_READ,
                                 vg_ble_attr_read, vg_ble_attr_write,
                                 VG_BLE_HANDLE_EVENT),
  GATT_H_CCCD_USER_RSP(GATT_PERM_READ | GATT_PERM_WRITE,
                       vg_ble_attr_read, vg_ble_attr_write,
                       VG_BLE_HANDLE_EVENT_CCC),
  GATT_H_CHARACTERISTIC_USER_RSP(g_vg_time_uuid,
                                 GATT_PROP_READ | GATT_PROP_WRITE,
                                 GATT_PERM_READ | GATT_PERM_WRITE,
                                 vg_ble_attr_read, vg_ble_attr_write,
                                 VG_BLE_HANDLE_TIME),
  GATT_H_CHARACTERISTIC_USER_RSP(g_vg_test_uuid,
                                 GATT_PROP_READ | GATT_PROP_WRITE |
                                 GATT_PROP_NOTIFY,
                                 GATT_PERM_READ | GATT_PERM_WRITE,
                                 vg_ble_attr_read, vg_ble_attr_write,
                                 VG_BLE_HANDLE_TEST),
  GATT_H_CCCD_USER_RSP(GATT_PERM_READ | GATT_PERM_WRITE,
                       vg_ble_attr_read, vg_ble_attr_write,
                       VG_BLE_HANDLE_TEST_CCC),
};

static gatt_srv_db_t g_vg_service_db =
{
  .attr_num = sizeof(g_vg_attrs) / sizeof(g_vg_attrs[0]),
  .attr_db = g_vg_attrs,
};

static uint8_t g_vg_adv_data[] =
{
  2, BT_AD_FLAGS,
  BT_AD_FLAG_GENERAL_DISCOVERABLE | BT_AD_FLAG_BREDR_NOT_SUPPORT,
  17, BT_AD_UUID128_ALL, VG_BLE_UUID_BYTES(0x01),
};

static uint8_t g_vg_scan_rsp_data[] =
{
  10, BT_AD_NAME_COMPLETE, 'V', 'e', 'l', 'a', 'G', 'u', 'a', 'r', 'd',
};

static ble_adv_params_t g_vg_adv_params =
{
  .adv_type = BT_LE_LEGACY_ADV_IND,
  .peer_addr_type = BT_LE_ADDR_TYPE_UNKNOWN,
  .own_addr_type = BT_LE_ADDR_TYPE_UNKNOWN,
  .tx_power = 0,
  .interval = 0x30,
  .duration = 0,
  .channel_map = BT_LE_ADV_CHANNEL_DEFAULT,
  .filter_policy = BT_LE_ADV_FILTER_WHITE_LIST_FOR_NONE,
};

static void vg_ble_gatts_connected(gatts_handle_t srv_handle,
                                   bt_address_t *addr)
{
  (void)srv_handle;

  g_vg_connected = true;
  g_vg_advertising = false;
  g_vg_adv_starting = false;
  g_vg_advertiser = NULL;
  if (addr != NULL)
    {
      g_vg_peer_addr = *addr;
      g_vg_peer_addr_valid = true;
    }

  printf("VelaGuard BLE: phone connected notify=%d pending=%d\n",
         g_vg_notify_enabled, g_vg_call_pending);
}

static void vg_ble_gatts_disconnected(gatts_handle_t srv_handle,
                                      bt_address_t *addr)
{
  (void)srv_handle;
  (void)addr;

  g_vg_connected = false;
  g_vg_peer_addr_valid = false;
  g_vg_notify_enabled = false;
  g_vg_test_notify_enabled = false;
  g_vg_test_probe_seen = false;
  g_vg_event_ccc_value = 0;
  g_vg_test_ccc_value = 0;
  g_vg_last_test_notify_ms = 0;
  g_vg_advertising = false;
  g_vg_adv_starting = false;
  g_vg_advertiser = NULL;

  if (g_vg_enabled)
    {
      g_vg_start_advertising_pending = true;
      g_vg_adv_retry_skip = VG_BLE_ADV_RETRY_SKIP;
      printf("VelaGuard BLE: phone disconnected; advertising restart scheduled\n");
    }
  else
    {
      printf("VelaGuard BLE: phone disconnected while disabled\n");
    }
}

static void vg_ble_attr_table_added(gatts_handle_t srv_handle,
                                    gatt_status_t status,
                                    uint16_t attr_handle)
{
  (void)srv_handle;

  if (status == GATT_STATUS_SUCCESS)
    {
      g_vg_attr_table_added = true;
    }

  printf("VelaGuard BLE: GATT attr table result status=%d handle=0x%04x\n",
         status, attr_handle);
}

static void vg_ble_notify_complete(gatts_handle_t srv_handle,
                                   bt_address_t *addr,
                                   gatt_status_t status,
                                   uint16_t attr_handle)
{
  (void)srv_handle;
  (void)addr;

  printf("VelaGuard BLE: notify complete handle=0x%04x status=%d\n",
         attr_handle, status);
}

static void vg_ble_mtu_changed(gatts_handle_t srv_handle,
                               bt_address_t *addr, uint32_t mtu)
{
  (void)srv_handle;
  (void)addr;
  printf("VelaGuard BLE: MTU changed mtu=%lu\n", (unsigned long)mtu);
}

static void vg_ble_conn_param_changed(gatts_handle_t srv_handle,
                                      bt_address_t *addr,
                                      uint16_t interval, uint16_t latency,
                                      uint16_t timeout)
{
  (void)srv_handle;
  (void)addr;
  printf("VelaGuard BLE: conn param interval=%u latency=%u timeout=%u\n",
         interval, latency, timeout);
}

static gatts_callbacks_t g_vg_gatts_callbacks =
{
  .size = sizeof(g_vg_gatts_callbacks),
  .on_connected = vg_ble_gatts_connected,
  .on_disconnected = vg_ble_gatts_disconnected,
  .on_attr_table_added = vg_ble_attr_table_added,
  .on_notify_complete = vg_ble_notify_complete,
  .on_mtu_changed = vg_ble_mtu_changed,
  .on_conn_param_changed = vg_ble_conn_param_changed,
};

static void vg_ble_adv_started(bt_advertiser_t *adv, uint8_t adv_id,
                               uint8_t status)
{
  if (status == BT_ADV_STATUS_SUCCESS)
    {
      g_vg_advertiser = adv;
      g_vg_advertising = true;
      g_vg_adv_starting = false;
      printf("VelaGuard BLE: advertising as %s id=%u\n",
             VG_BLE_NAME, adv_id);
      return;
    }

  g_vg_advertiser = NULL;
  g_vg_advertising = false;
  g_vg_adv_starting = false;
  if (g_vg_enabled && !g_vg_connected)
    {
      g_vg_start_advertising_pending = true;
      g_vg_adv_retry_skip = VG_BLE_ADV_RETRY_SKIP;
    }

  printf("VelaGuard BLE: advertising start failed status=%u\n", status);
}

static void vg_ble_adv_stopped(bt_advertiser_t *adv, uint8_t adv_id)
{
  (void)adv;
  g_vg_advertiser = NULL;
  g_vg_advertising = false;
  g_vg_adv_starting = false;
  printf("VelaGuard BLE: advertising stopped id=%u\n", adv_id);
}

static advertiser_callback_t g_vg_adv_callbacks =
{
  .size = sizeof(g_vg_adv_callbacks),
  .on_advertising_start = vg_ble_adv_started,
  .on_advertising_stopped = vg_ble_adv_stopped,
};

static void vg_ble_adapter_state_changed(void *cookie,
                                         bt_adapter_state_t state)
{
  (void)cookie;

  g_vg_adapter_ready =
    state == BT_ADAPTER_STATE_ON || state == BT_ADAPTER_STATE_BLE_ON;
  if (g_vg_adapter_ready)
    {
      g_vg_start_advertising_pending = true;
      g_vg_adv_retry_skip = 0;
    }
  printf("VelaGuard BLE: adapter state=%d ready=%d\n", state,
         g_vg_adapter_ready ? 1 : 0);
}

static adapter_callbacks_t g_vg_adapter_callbacks =
{
  .on_adapter_state_changed = vg_ble_adapter_state_changed,
};

static int vg_ble_register_service_once(void)
{
  bt_status_t status;
  int ret;

  if (!g_vg_initialized || !g_vg_enabled)
    {
      return -ENODEV;
    }

  if (g_vg_service_registered)
    {
      return 0;
    }

  if (!vg_ble_framework_ready())
    {
      return -EAGAIN;
    }

  status = bt_adapter_set_name(g_vg_bt, VG_BLE_NAME);
  ret = vg_ble_status_to_errno(status);
  if (ret < 0 && status != BT_STATUS_DONE)
    {
      printf("VelaGuard BLE: set name failed status=%d\n", status);
    }

  status = bt_gatts_register_service(g_vg_bt, &g_vg_gatts,
                                     &g_vg_gatts_callbacks);
  ret = vg_ble_status_to_errno(status);
  if (ret < 0 || g_vg_gatts == NULL)
    {
      printf("VelaGuard BLE: GATT service registration failed status=%d\n",
             status);
      return ret < 0 ? ret : -EIO;
    }

  status = bt_gatts_add_attr_table(g_vg_gatts, &g_vg_service_db);
  ret = vg_ble_status_to_errno(status);
  if (ret < 0)
    {
      printf("VelaGuard BLE: GATT attr table failed status=%d\n", status);
      return ret;
    }

  g_vg_service_registered = true;
  vg_ble_cache_local_address();
  printf("VelaGuard BLE: GATT service registered through framework\n");
  return 0;
}

static int vg_ble_start_advertising(void)
{
  if (!g_vg_initialized || !g_vg_enabled)
    {
      return -ENODEV;
    }

  if (g_vg_connected || g_vg_advertising || g_vg_adv_starting)
    {
      return 0;
    }

  if (!g_vg_service_registered)
    {
      return -EAGAIN;
    }

  g_vg_advertiser = bt_le_start_advertising(g_vg_bt, &g_vg_adv_params,
                                            g_vg_adv_data,
                                            sizeof(g_vg_adv_data),
                                            g_vg_scan_rsp_data,
                                            sizeof(g_vg_scan_rsp_data),
                                            &g_vg_adv_callbacks);
  if (g_vg_advertiser == NULL)
    {
      printf("VelaGuard BLE: advertising request failed\n");
      return -EIO;
    }

  g_vg_adv_starting = true;
  printf("VelaGuard BLE: advertising start requested\n");
  return 0;
}

int vg_ble_init(void)
{
  bt_status_t status;
  int ret;

  if (g_vg_initialized)
    {
      return 0;
    }

  memset(&g_vg_last_packet, 0, sizeof(g_vg_last_packet));
  strlcpy(g_vg_local_addr, "pending", sizeof(g_vg_local_addr));
  printf("VelaGuard BLE: framework init start\n");

  g_vg_bt = bluetooth_create_instance();
  if (g_vg_bt == NULL)
    {
      printf("VelaGuard BLE: bluetooth instance create failed\n");
      return -ENOMEM;
    }

  bt_adapter_register_callback(g_vg_bt, &g_vg_adapter_callbacks);

  status = bt_adapter_enable_le(g_vg_bt);
  ret = vg_ble_status_to_errno(status);
  if (ret < 0)
    {
      printf("VelaGuard BLE: adapter enable LE failed status=%d\n", status);
      return ret;
    }

  g_vg_initialized = true;
  g_vg_enabled = true;
  g_vg_adapter_ready = vg_ble_framework_ready();
  g_vg_start_advertising_pending = g_vg_adapter_ready;
  printf("VelaGuard BLE: framework enable requested status=%d ready=%d\n",
         status, g_vg_adapter_ready ? 1 : 0);
  return 0;
}

void vg_ble_process(void)
{
  int ret;

  if (g_vg_enable_request_pending)
    {
      bool target = g_vg_enable_request_target;

      g_vg_enable_request_pending = false;
      ret = vg_ble_set_enabled(target);
      printf("VelaGuard BLE: async enable target=%d ret=%d enabled=%d adv=%d connected=%d\n",
             target ? 1 : 0, ret, g_vg_enabled ? 1 : 0,
             g_vg_advertising ? 1 : 0, g_vg_connected ? 1 : 0);
    }

  if (!g_vg_initialized || !g_vg_enabled)
    {
      return;
    }

  if (!g_vg_adapter_ready && vg_ble_framework_ready())
    {
      g_vg_adapter_ready = true;
      g_vg_start_advertising_pending = true;
      g_vg_adv_retry_skip = 0;
      printf("VelaGuard BLE: adapter ready by poll\n");
    }

  if (g_vg_adapter_ready && !g_vg_service_registered)
    {
      ret = vg_ble_register_service_once();
      if (ret == 0)
        {
          g_vg_start_advertising_pending = true;
        }
    }

  if (g_vg_start_advertising_pending && g_vg_adapter_ready &&
      g_vg_service_registered && !g_vg_connected)
    {
      if (g_vg_adv_retry_skip > 0)
        {
          g_vg_adv_retry_skip--;
        }
      else
        {
          g_vg_start_advertising_pending = false;
          ret = vg_ble_start_advertising();
          if (ret < 0)
            {
              g_vg_start_advertising_pending = true;
              g_vg_adv_retry_skip = VG_BLE_ADV_RETRY_SKIP;
              printf("VelaGuard BLE: advertising retry ret=%d\n", ret);
            }
        }
    }

  if (g_vg_call_pending && g_vg_connected && g_vg_notify_enabled &&
      g_vg_peer_addr_valid && g_vg_service_registered)
    {
      struct vg_ble_call_packet_s packet;
      irqstate_t irqstate;
      bt_status_t status;

      irqstate = enter_critical_section();
      packet = g_vg_last_packet;
      leave_critical_section(irqstate);

      status = bt_gatts_notify(g_vg_gatts, &g_vg_peer_addr,
                               VG_BLE_HANDLE_EVENT,
                               (uint8_t *)&packet, sizeof(packet));
      ret = vg_ble_status_to_errno(status);
      printf("VelaGuard BLE: CALL_REQUEST id=%lu result=%d status=%d\n",
             (unsigned long)packet.event_id, ret, status);
      if (ret == 0)
        {
          irqstate = enter_critical_section();
          g_vg_call_pending = false;
          leave_critical_section(irqstate);
        }
    }

  if (g_vg_connected && g_vg_test_notify_enabled && g_vg_test_probe_seen &&
      g_vg_peer_addr_valid && g_vg_service_registered)
    {
      uint64_t now_ms = vg_ble_monotonic_ms();

      if (g_vg_last_test_notify_ms == 0 ||
          now_ms - g_vg_last_test_notify_ms >= VG_BLE_TEST_NOTIFY_MS)
        {
          uint8_t payload[4];
          bt_status_t status;

          g_vg_test_counter++;
          vg_ble_put_le32(payload, g_vg_test_counter);
          status = bt_gatts_notify(g_vg_gatts, &g_vg_peer_addr,
                                   VG_BLE_HANDLE_TEST, payload,
                                   sizeof(payload));
          ret = vg_ble_status_to_errno(status);
          printf("VelaGuard BLE TEST: notify counter=%lu result=%d status=%d\n",
                 (unsigned long)g_vg_test_counter, ret, status);
          if (ret == 0)
            {
              g_vg_last_test_notify_ms = now_ms;
            }
        }
    }
}

void vg_ble_process_ui(void)
{
  struct timespec ts;
  uint64_t epoch_ms = 0;
  irqstate_t irqstate;
  int ret;

  if (!g_vg_time_sync_pending)
    {
      return;
    }

  irqstate = enter_critical_section();
  if (g_vg_time_sync_pending)
    {
      epoch_ms = g_vg_pending_time_sync_ms;
      g_vg_time_sync_pending = false;
    }
  leave_critical_section(irqstate);

  if (epoch_ms == 0)
    {
      return;
    }

  ts.tv_sec = epoch_ms / 1000;
  ts.tv_nsec = (epoch_ms % 1000) * 1000000;
  ret = clock_settime(CLOCK_REALTIME, &ts);
  if (ret < 0)
    {
      printf("VelaGuard BLE: time sync failed epoch_ms=%llu ret=%d errno=%d\n",
             (unsigned long long)epoch_ms, ret, errno);
    }
  else
    {
      g_vg_last_time_sync_ms = epoch_ms;
      printf("VelaGuard BLE: time synced epoch_ms=%llu\n",
             (unsigned long long)epoch_ms);
    }
}

void vg_ble_process_time(void)
{
  vg_ble_process_ui();
}

void vg_ble_set_fall_status(bool active)
{
  UNUSED(active);
}

bool vg_ble_is_connected(void)
{
  return g_vg_connected;
}

bool vg_ble_is_initialized(void)
{
  return g_vg_initialized;
}

bool vg_ble_is_enabled(void)
{
  return g_vg_enabled;
}

bool vg_ble_is_ready(void)
{
  return g_vg_initialized && g_vg_enabled && g_vg_adapter_ready &&
         g_vg_service_registered;
}

void vg_ble_get_device_name(char *buf, size_t len)
{
  if (buf != NULL && len != 0)
    {
      strlcpy(buf, "VelaGuard", len);
    }
}

bool vg_ble_is_advertising(void)
{
  return g_vg_advertising || g_vg_adv_starting;
}

bool vg_ble_has_pending_enable_request(void)
{
  return g_vg_enable_request_pending;
}

int vg_ble_set_enabled(bool enabled)
{
  int ret = 0;

  if (enabled)
    {
      if (!g_vg_initialized)
        {
          return vg_ble_init();
        }

      if (g_vg_enabled)
        {
          return 0;
        }

      g_vg_enabled = true;
      g_vg_start_advertising_pending = true;
      g_vg_adv_retry_skip = 0;
      return 0;
    }

  if (!g_vg_initialized || !g_vg_enabled)
    {
      return 0;
    }

  g_vg_enabled = false;
  g_vg_start_advertising_pending = false;
  g_vg_adv_retry_skip = 0;
  g_vg_call_pending = false;
  g_vg_advertising = false;
  g_vg_adv_starting = false;

  if (g_vg_connected && g_vg_peer_addr_valid && g_vg_gatts != NULL)
    {
      ret = vg_ble_status_to_errno(bt_gatts_disconnect(g_vg_gatts,
                                                       &g_vg_peer_addr));
      if (ret < 0)
        {
          printf("VelaGuard BLE: disconnect failed (%d)\n", ret);
        }
    }

  if (g_vg_advertiser != NULL)
    {
      bt_le_stop_advertising(g_vg_bt, g_vg_advertiser);
      g_vg_advertiser = NULL;
    }

  printf("VelaGuard BLE: disabled ret=%d\n", ret);
  return ret;
}

void vg_ble_request_set_enabled(bool enabled)
{
  g_vg_enable_request_target = enabled;
  g_vg_enable_request_pending = true;
}

void vg_ble_get_local_address(char *buf, size_t len)
{
  if (buf == NULL || len == 0)
    {
      return;
    }

  if (!g_vg_initialized)
    {
      strlcpy(buf, "initializing", len);
      return;
    }

  if (g_vg_local_addr[0] == '\0' ||
      strcmp(g_vg_local_addr, "pending") == 0)
    {
      vg_ble_cache_local_address();
    }

  strlcpy(buf, g_vg_local_addr[0] == '\0' ? "pending" :
          g_vg_local_addr, len);
}

bool vg_ble_local_address_is_default(void)
{
  return g_vg_local_addr_default;
}

int vg_ble_request_call(uint8_t event_type, uint8_t risk,
                        uint8_t confidence, uint32_t event_id,
                        uint32_t uptime_ms, bool user_confirmed)
{
  struct vg_ble_call_packet_s packet;
  irqstate_t irqstate;

  if (!g_vg_initialized || !g_vg_enabled)
    {
      return -EHOSTDOWN;
    }

  packet.magic[0] = 'V';
  packet.magic[1] = 'G';
  packet.version = 1;
  packet.command = VG_BLE_COMMAND_CALL_REQUEST;
  packet.event_type = event_type;
  packet.risk = risk;
  packet.confidence = confidence;
  packet.flags = user_confirmed ? VG_BLE_FLAG_USER_CONFIRMED : 0;
  packet.event_id = event_id;
  packet.uptime_ms = uptime_ms;

  printf("VelaGuard BLE: CALL_REQUEST packet=%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
         packet.magic[0], packet.magic[1], packet.version, packet.command,
         packet.event_type, packet.risk, packet.confidence, packet.flags,
         ((uint8_t *)&packet.event_id)[0], ((uint8_t *)&packet.event_id)[1],
         ((uint8_t *)&packet.event_id)[2], ((uint8_t *)&packet.event_id)[3],
         ((uint8_t *)&packet.uptime_ms)[0], ((uint8_t *)&packet.uptime_ms)[1],
         ((uint8_t *)&packet.uptime_ms)[2], ((uint8_t *)&packet.uptime_ms)[3]);

  irqstate = enter_critical_section();
  g_vg_last_packet = packet;
  g_vg_call_pending = true;
  leave_critical_section(irqstate);

  if (!g_vg_connected || !g_vg_notify_enabled)
    {
      printf("VelaGuard BLE: call request queued; phone not ready\n");
      return -ENOTCONN;
    }

  printf("VelaGuard BLE: CALL_REQUEST id=%lu queued\n",
         (unsigned long)event_id);
  return 0;
}
