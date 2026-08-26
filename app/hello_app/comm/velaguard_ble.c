/****************************************************************************
 * VelaGuard BLE emergency-call transport.
 ****************************************************************************/

#include <nuttx/config.h>

#include "velaguard_ble.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

#define VG_BLE_SERVICE_UUID \
  BT_UUID_128_ENCODE(0x6f70656e, 0x7665, 0x4c61, 0x9361, 0x726456470001)
#define VG_BLE_EVENT_UUID \
  BT_UUID_128_ENCODE(0x6f70656e, 0x7665, 0x4c61, 0x9361, 0x726456470002)

#define VG_BLE_COMMAND_CALL_REQUEST 1
#define VG_BLE_FLAG_USER_CONFIRMED  1
#define VG_BLE_ADDR_TEXT_LEN        18

static const struct bt_uuid_128 g_vg_service_uuid =
  BT_UUID_INIT_128(VG_BLE_SERVICE_UUID);
static const struct bt_uuid_128 g_vg_event_uuid =
  BT_UUID_INIT_128(VG_BLE_EVENT_UUID);

static struct vg_ble_call_packet_s g_vg_last_packet;
static struct bt_conn *g_vg_conn;
static bool g_vg_connected;
static bool g_vg_notify_enabled;
static bool g_vg_initialized;
static bool g_vg_enabled;
static bool g_vg_call_pending;
static volatile bool g_vg_restart_advertising;
static unsigned int g_vg_adv_retry_skip;
static char g_vg_local_addr[VG_BLE_ADDR_TEXT_LEN];

/* The zblue H:4 port always references the optional snoop hook, while this
 * compact contest configuration does not include the Bluetooth log service.
 */

void btsnoop_log_capture(uint8_t is_receive, uint8_t *packet,
                         uint32_t packet_size)
{
  (void)is_receive;
  (void)packet;
  (void)packet_size;
}

static ssize_t vg_ble_read_event(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr,
                                 void *buf, uint16_t len, uint16_t offset)
{
  return bt_gatt_attr_read(conn, attr, buf, len, offset,
                           &g_vg_last_packet, sizeof(g_vg_last_packet));
}

static void vg_ble_ccc_changed(const struct bt_gatt_attr *attr,
                               uint16_t value)
{
  g_vg_notify_enabled = value == BT_GATT_CCC_NOTIFY;
  printf("VelaGuard BLE: phone notifications %s\n",
         g_vg_notify_enabled ? "enabled" : "disabled");
}

/* zblue's NuttX port uses a fixed, manually maintained list for static GATT
 * services, so application-defined iterable-section services are not picked
 * up automatically.  Keep the attributes mutable and register this service
 * explicitly after bt_enable().
 */

static struct bt_gatt_attr g_vg_attrs[] =
{
  BT_GATT_PRIMARY_SERVICE(&g_vg_service_uuid),
  BT_GATT_CHARACTERISTIC(&g_vg_event_uuid.uuid,
                         BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                         BT_GATT_PERM_READ,
                         vg_ble_read_event, NULL, &g_vg_last_packet),
  BT_GATT_CCC(vg_ble_ccc_changed,
              BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
};

static struct bt_gatt_service g_vg_service = BT_GATT_SERVICE(g_vg_attrs);

static void vg_ble_connected(struct bt_conn *conn, uint8_t err)
{
  if (err == 0)
    {
      g_vg_connected = true;
      g_vg_conn = bt_conn_ref(conn);
      printf("VelaGuard BLE: phone connected\n");
    }
  else
    {
      printf("VelaGuard BLE: connection failed (%u)\n", err);
    }
}

static void vg_ble_disconnected(struct bt_conn *conn, uint8_t reason)
{
  g_vg_connected = false;
  g_vg_notify_enabled = false;
  if (g_vg_conn != NULL)
    {
      bt_conn_unref(g_vg_conn);
      g_vg_conn = NULL;
    }

  if (g_vg_enabled)
    {
      g_vg_restart_advertising = true;
      g_vg_adv_retry_skip = 0;
      printf("VelaGuard BLE: advertising restart scheduled\n");
    }

  printf("VelaGuard BLE: phone disconnected (%u)\n", reason);
}

static struct bt_conn_cb g_vg_conn_callbacks =
{
  .connected = vg_ble_connected,
  .disconnected = vg_ble_disconnected,
};

static void vg_ble_pairing_complete(struct bt_conn *conn, bool bonded)
{
  printf("VelaGuard BLE: pairing complete bonded=%d\n", bonded);
}

static void vg_ble_pairing_failed(struct bt_conn *conn,
                                  enum bt_security_err reason)
{
  printf("VelaGuard BLE: pairing failed reason=%d; GATT may remain connected\n",
         reason);
}

static struct bt_conn_auth_info_cb g_vg_auth_info_callbacks =
{
  .pairing_complete = vg_ble_pairing_complete,
  .pairing_failed = vg_ble_pairing_failed,
};

static const struct bt_data g_vg_ad[] =
{
  BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
  BT_DATA_BYTES(BT_DATA_UUID128_ALL, VG_BLE_SERVICE_UUID),
};

static const struct bt_data g_vg_sd[] =
{
  BT_DATA(BT_DATA_NAME_COMPLETE, "VelaGuard", 9),
};

static bool vg_ble_addr_is_zero(const bt_addr_le_t *addr)
{
  int i;

  for (i = 0; i < 6; i++)
    {
      if (addr->a.val[i] != 0)
        {
          return false;
        }
    }

  return true;
}

static void vg_ble_cache_local_address(void)
{
  bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
  size_t count = ARRAY_SIZE(addrs);

  strlcpy(g_vg_local_addr, "pending", sizeof(g_vg_local_addr));

  if (!g_vg_initialized)
    {
      return;
    }

  bt_id_get(addrs, &count);
  if (count == 0 || vg_ble_addr_is_zero(&addrs[0]))
    {
      printf("VelaGuard BLE: local identity address unavailable\n");
      return;
    }

  bt_addr_to_str(&addrs[0].a, g_vg_local_addr, sizeof(g_vg_local_addr));
  printf("VelaGuard BLE: local address %s\n", g_vg_local_addr);
}

static int vg_ble_start_advertising(void)
{
  int ret;

  ret = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1,
                        g_vg_ad, ARRAY_SIZE(g_vg_ad),
                        g_vg_sd, ARRAY_SIZE(g_vg_sd));
  if (ret == -EALREADY)
    {
      printf("VelaGuard BLE: advertising already active; restarting\n");
      bt_le_adv_stop();
      ret = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1,
                            g_vg_ad, ARRAY_SIZE(g_vg_ad),
                            g_vg_sd, ARRAY_SIZE(g_vg_sd));
    }

  if (ret < 0)
    {
      printf("VelaGuard BLE: advertising failed (%d)\n", ret);
      return ret;
    }

  printf("VelaGuard BLE: advertising as VelaGuard\n");
  return 0;
}

int vg_ble_init(void)
{
  int ret;

  if (g_vg_initialized)
    {
      return 0;
    }

  memset(&g_vg_last_packet, 0, sizeof(g_vg_last_packet));
  ret = bt_enable(NULL);
  if (ret < 0 && ret != -EALREADY)
    {
      printf("VelaGuard BLE: stack init failed (%d)\n", ret);
      return ret;
    }

  ret = bt_conn_cb_register(&g_vg_conn_callbacks);
  if (ret < 0 && ret != -EEXIST)
    {
      printf("VelaGuard BLE: connection callback registration failed (%d)\n",
             ret);
      return ret;
    }

  ret = bt_conn_auth_info_cb_register(&g_vg_auth_info_callbacks);
  if (ret < 0 && ret != -EEXIST)
    {
      printf("VelaGuard BLE: auth callback registration failed (%d)\n", ret);
      return ret;
    }

  ret = bt_gatt_service_register(&g_vg_service);
  if (ret < 0 && ret != -EALREADY)
    {
      printf("VelaGuard BLE: GATT service registration failed (%d)\n", ret);
      return ret;
    }

  printf("VelaGuard BLE: GATT service registered\n");

  g_vg_initialized = true;
  g_vg_enabled = true;
  vg_ble_cache_local_address();

  ret = vg_ble_start_advertising();
  if (ret < 0)
    {
      g_vg_enabled = false;
      return ret;
    }

  return 0;
}

void vg_ble_process(void)
{
  int ret;

  if (!g_vg_initialized || !g_vg_enabled)
    {
      return;
    }

  /* A user may press SOS while Android is still discovering services and
   * writing the CCC descriptor.  Keep the newest emergency packet queued and
   * deliver it as soon as the connected phone has enabled notifications.
   */

  if (g_vg_call_pending && g_vg_connected && g_vg_notify_enabled)
    {
      ret = bt_gatt_notify(NULL, &g_vg_service.attrs[2],
                           &g_vg_last_packet, sizeof(g_vg_last_packet));
      printf("VelaGuard BLE: CALL_REQUEST id=%lu result=%d\n",
             (unsigned long)g_vg_last_packet.event_id, ret);
      if (ret == 0)
        {
          g_vg_call_pending = false;
        }
    }

  if (!g_vg_restart_advertising)
    {
      return;
    }

  if (g_vg_adv_retry_skip > 0)
    {
      g_vg_adv_retry_skip--;
      return;
    }

  /* Clear before issuing HCI commands so a fresh disconnect callback that
   * arrives during startup can set the flag again without being overwritten.
   */

  g_vg_restart_advertising = false;

  /* With extended-advertising capable controllers zblue keeps the legacy
   * wrapper object after a connection, although that object is no longer
   * transmitting.  Starting again then returns -EALREADY without issuing an
   * HCI enable command.  Stop first to discard that stale wrapper object.
   */

  ret = bt_le_adv_stop();
  if (ret < 0)
    {
      printf("VelaGuard BLE: stale advertising cleanup failed (%d)\n", ret);
    }
  else
    {
      printf("VelaGuard BLE: stale advertising state cleared\n");
      ret = vg_ble_start_advertising();
    }

  if (ret == 0)
    {
      printf("VelaGuard BLE: advertising restarted after disconnect\n");
    }
  else
    {
      /* Retry outside the Bluetooth callback context.  The exact delay
       * depends on the UI/headless loop period, but prevents a tight HCI
       * command retry loop if the controller is temporarily busy.
       */

      g_vg_restart_advertising = true;
      g_vg_adv_retry_skip = 50;
    }
}

bool vg_ble_is_connected(void)
{
  return g_vg_connected;
}

bool vg_ble_is_enabled(void)
{
  return g_vg_enabled;
}

int vg_ble_set_enabled(bool enabled)
{
  int ret;

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
      ret = vg_ble_start_advertising();
      if (ret < 0)
        {
          g_vg_enabled = false;
        }

      return ret;
    }

  if (!g_vg_initialized || !g_vg_enabled)
    {
      return 0;
    }

  g_vg_enabled = false;
  g_vg_restart_advertising = false;
  g_vg_adv_retry_skip = 0;
  g_vg_call_pending = false;

  if (g_vg_connected && g_vg_conn != NULL)
    {
      ret = bt_conn_disconnect(g_vg_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
      if (ret < 0)
        {
          printf("VelaGuard BLE: disconnect failed (%d)\n", ret);
          return ret;
        }
    }

  ret = bt_le_adv_stop();
  if (ret < 0 && ret != -EALREADY)
    {
      printf("VelaGuard BLE: advertising stop failed (%d)\n", ret);
      return ret;
    }

  printf("VelaGuard BLE: disabled\n");
  return 0;
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

int vg_ble_request_call(uint8_t event_type, uint8_t risk,
                        uint8_t confidence, uint32_t event_id,
                        uint32_t uptime_ms, bool user_confirmed)
{
  if (!g_vg_initialized || !g_vg_enabled)
    {
      return -EHOSTDOWN;
    }

  g_vg_last_packet.magic[0] = 'V';
  g_vg_last_packet.magic[1] = 'G';
  g_vg_last_packet.version = 1;
  g_vg_last_packet.command = VG_BLE_COMMAND_CALL_REQUEST;
  g_vg_last_packet.event_type = event_type;
  g_vg_last_packet.risk = risk;
  g_vg_last_packet.confidence = confidence;
  g_vg_last_packet.flags = user_confirmed ? VG_BLE_FLAG_USER_CONFIRMED : 0;
  g_vg_last_packet.event_id = event_id;
  g_vg_last_packet.uptime_ms = uptime_ms;
  g_vg_call_pending = true;

  if (!g_vg_connected || !g_vg_notify_enabled)
    {
      printf("VelaGuard BLE: call request queued; phone not ready\n");
      return -ENOTCONN;
    }

  /* Do not issue a synchronous GATT/HCI operation from an LVGL callback.
   * vg_ble_process() sends this queued packet after the UI handler returns.
   */

  printf("VelaGuard BLE: CALL_REQUEST id=%lu queued\n",
         (unsigned long)event_id);
  return 0;
}
