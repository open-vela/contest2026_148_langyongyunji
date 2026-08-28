/****************************************************************************
 * VelaGuard BLE emergency-call transport.
 ****************************************************************************/

#include <nuttx/config.h>

#include "velaguard_ble.h"

#include <errno.h>
#include <nuttx/irq.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

#define VG_BLE_SERVICE_UUID \
  BT_UUID_128_ENCODE(0x6f70656e, 0x7665, 0x4c61, 0x9361, 0x726456470001)
#define VG_BLE_EVENT_UUID \
  BT_UUID_128_ENCODE(0x6f70656e, 0x7665, 0x4c61, 0x9361, 0x726456470002)
#define VG_BLE_TIME_UUID \
  BT_UUID_128_ENCODE(0x6f70656e, 0x7665, 0x4c61, 0x9361, 0x726456470003)
#define VG_BLE_TEST_UUID \
  BT_UUID_128_ENCODE(0x6f70656e, 0x7665, 0x4c61, 0x9361, 0x726456470004)

#define VG_BLE_COMMAND_CALL_REQUEST 1
#define VG_BLE_FLAG_USER_CONFIRMED  1
#define VG_BLE_ADDR_TEXT_LEN        18
#define VG_BLE_TEST_NOTIFY_MS       1000
#define VG_BLE_ADV_RESTART_SKIP     250
#define VG_BLE_EFUSE_SEED_OFFSET    256
#define VG_BLE_EFUSE_SEED_LEN       32
#define VG_BLE_FACTORY_CFG_ID_MAC   1
#define VG_BLE_HAL_OK               0

enum vg_ble_attr_index_e
{
  VG_BLE_ATTR_SERVICE = 0,
  VG_BLE_ATTR_EVENT_CHRC,
  VG_BLE_ATTR_EVENT_VALUE,
  VG_BLE_ATTR_EVENT_CCC,
  VG_BLE_ATTR_TIME_CHRC,
  VG_BLE_ATTR_TIME_VALUE,
  VG_BLE_ATTR_TEST_CHRC,
  VG_BLE_ATTR_TEST_VALUE,
  VG_BLE_ATTR_TEST_CCC,
};

static const struct bt_uuid_128 g_vg_service_uuid =
  BT_UUID_INIT_128(VG_BLE_SERVICE_UUID);
static const struct bt_uuid_128 g_vg_event_uuid =
  BT_UUID_INIT_128(VG_BLE_EVENT_UUID);
static const struct bt_uuid_128 g_vg_time_uuid =
  BT_UUID_INIT_128(VG_BLE_TIME_UUID);
static const struct bt_uuid_128 g_vg_test_uuid =
  BT_UUID_INIT_128(VG_BLE_TEST_UUID);

static struct vg_ble_call_packet_s g_vg_last_packet;
static struct bt_conn *g_vg_conn;
static bool g_vg_connected;
static bool g_vg_notify_enabled;
static bool g_vg_initialized;
static bool g_vg_enabled;
static bool g_vg_advertising;
static bool g_vg_call_pending;
static bool g_vg_local_addr_default;
static bool g_vg_identity_configured;
static volatile bool g_vg_test_notify_enabled;
static volatile bool g_vg_test_probe_seen;
static volatile bool g_vg_enable_request_pending;
static volatile bool g_vg_enable_request_target;
static volatile bool g_vg_restart_advertising;
static volatile bool g_vg_time_sync_pending;
static unsigned int g_vg_adv_retry_skip;
static char g_vg_local_addr[VG_BLE_ADDR_TEXT_LEN];
static uint64_t g_vg_last_time_sync_ms;
static uint64_t g_vg_pending_time_sync_ms;
static uint64_t g_vg_last_test_notify_ms;
static uint32_t g_vg_test_counter;
static uint8_t g_vg_last_test_value;

extern int HAL_EFUSE_Init(void);
extern int32_t HAL_EFUSE_Read(uint16_t bit_offset, uint8_t *data, int size);
extern int BSP_CONFIG_get(int type, uint8_t *buf, int length);

static void vg_ble_drop_connection_ref(void);

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

static uint64_t vg_ble_hash_seed(const uint8_t *data, size_t len)
{
  uint64_t hash = 1469598103934665603ULL;
  size_t i;

  for (i = 0; i < len; i++)
    {
      hash ^= data[i];
      hash *= 1099511628211ULL;
    }

  return hash;
}

static bool vg_ble_seed_is_valid(const uint8_t *data, size_t len)
{
  bool any_nonzero = false;
  bool any_nonff = false;
  size_t i;

  for (i = 0; i < len; i++)
    {
      any_nonzero |= data[i] != 0x00;
      any_nonff |= data[i] != 0xff;
    }

  return any_nonzero && any_nonff;
}

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

static ssize_t vg_ble_read_time(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr,
                                void *buf, uint16_t len, uint16_t offset)
{
  return bt_gatt_attr_read(conn, attr, buf, len, offset,
                           &g_vg_last_time_sync_ms,
                           sizeof(g_vg_last_time_sync_ms));
}

static ssize_t vg_ble_write_time(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr,
                                 const void *buf, uint16_t len,
                                 uint16_t offset, uint8_t flags)
{
  const uint8_t *bytes = buf;
  uint64_t epoch_ms;
  irqstate_t irqstate;

  UNUSED(conn);
  UNUSED(attr);
  UNUSED(flags);

  if (offset != 0)
    {
      return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

  if (len != sizeof(epoch_ms))
    {
      printf("VelaGuard BLE: time sync invalid len=%u\n", len);
      return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

  epoch_ms = vg_ble_get_le64(bytes);

  irqstate = enter_critical_section();
  g_vg_pending_time_sync_ms = epoch_ms;
  g_vg_time_sync_pending = true;
  leave_critical_section(irqstate);

  printf("VelaGuard BLE: time sync queued epoch_ms=%llu\n",
         (unsigned long long)epoch_ms);
  return len;
}

static ssize_t vg_ble_read_test(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr,
                                void *buf, uint16_t len, uint16_t offset)
{
  return bt_gatt_attr_read(conn, attr, buf, len, offset,
                           &g_vg_last_test_value,
                           sizeof(g_vg_last_test_value));
}

static ssize_t vg_ble_write_test(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr,
                                 const void *buf, uint16_t len,
                                 uint16_t offset, uint8_t flags)
{
  const uint8_t *bytes = buf;

  UNUSED(conn);
  UNUSED(attr);
  UNUSED(flags);

  if (offset != 0)
    {
      return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

  if (len < 1)
    {
      return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

  g_vg_last_test_value = bytes[0];
  g_vg_test_probe_seen = true;
  g_vg_last_test_notify_ms = 0;
  printf("VelaGuard BLE TEST: write len=%u first=0x%02x\n",
         len, g_vg_last_test_value);
  return len;
}

static void vg_ble_ccc_changed(const struct bt_gatt_attr *attr,
                               uint16_t value)
{
  g_vg_notify_enabled = value == BT_GATT_CCC_NOTIFY;
  printf("VelaGuard BLE: CCC changed handle=0x%04x value=0x%04x notifications=%s\n",
         attr != NULL ? attr->handle : 0, value,
         g_vg_notify_enabled ? "enabled" : "disabled");
}

static void vg_ble_test_ccc_changed(const struct bt_gatt_attr *attr,
                                    uint16_t value)
{
  g_vg_test_notify_enabled = value == BT_GATT_CCC_NOTIFY;
  if (g_vg_test_notify_enabled)
    {
      g_vg_last_test_notify_ms = 0;
    }

  printf("VelaGuard BLE TEST: CCC changed handle=0x%04x value=0x%04x notifications=%s\n",
         attr != NULL ? attr->handle : 0, value,
         g_vg_test_notify_enabled ? "enabled" : "disabled");
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
  BT_GATT_CHARACTERISTIC(&g_vg_time_uuid.uuid,
                         BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                         BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                         vg_ble_read_time, vg_ble_write_time,
                         &g_vg_last_time_sync_ms),
  BT_GATT_CHARACTERISTIC(&g_vg_test_uuid.uuid,
                         BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE |
                         BT_GATT_CHRC_NOTIFY,
                         BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                         vg_ble_read_test, vg_ble_write_test,
                         &g_vg_last_test_value),
  BT_GATT_CCC(vg_ble_test_ccc_changed,
              BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
};

static struct bt_gatt_service g_vg_service = BT_GATT_SERVICE(g_vg_attrs);

static void vg_ble_connected(struct bt_conn *conn, uint8_t err)
{
  if (err == 0)
    {
      g_vg_connected = true;
      g_vg_advertising = false;
      g_vg_conn = bt_conn_ref(conn);
      printf("VelaGuard BLE: phone connected notify=%d pending=%d\n",
             g_vg_notify_enabled, g_vg_call_pending);
    }
  else
    {
      UNUSED(conn);
      printf("VelaGuard BLE: connection failed err=%u\n", err);
    }
}

static void vg_ble_disconnected(struct bt_conn *conn, uint8_t reason)
{
  UNUSED(conn);
  g_vg_connected = false;
  g_vg_notify_enabled = false;
  g_vg_test_notify_enabled = false;
  g_vg_test_probe_seen = false;
  g_vg_last_test_notify_ms = 0;
  vg_ble_drop_connection_ref();

  g_vg_advertising = false;
  printf("VelaGuard BLE: phone disconnected reason=%u\n", reason);
  if (g_vg_enabled)
    {
      printf("VelaGuard BLE: wait for conn recycled before advertising restart\n");
    }
}

static void vg_ble_recycled(void)
{
  if (g_vg_enabled && !g_vg_connected)
    {
      g_vg_restart_advertising = true;
      g_vg_adv_retry_skip = VG_BLE_ADV_RESTART_SKIP;
      printf("VelaGuard BLE: advertising restart scheduled after conn recycled\n");
    }
}

static bool vg_ble_le_param_req(struct bt_conn *conn,
                                struct bt_le_conn_param *param)
{
  UNUSED(conn);
  UNUSED(param);
  return true;
}

static void vg_ble_le_param_updated(struct bt_conn *conn, uint16_t interval,
                                    uint16_t latency, uint16_t timeout)
{
  UNUSED(conn);
  UNUSED(interval);
  UNUSED(latency);
  UNUSED(timeout);
}

static void vg_ble_security_changed(struct bt_conn *conn, bt_security_t level,
                                    enum bt_security_err err)
{
  UNUSED(conn);
  UNUSED(level);
  UNUSED(err);
}

#if defined(CONFIG_BT_USER_PHY_UPDATE)
static void vg_ble_le_phy_updated(struct bt_conn *conn,
                                  struct bt_conn_le_phy_info *param)
{
  UNUSED(conn);
  UNUSED(param);
}
#endif

#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
static void vg_ble_le_data_len_updated(struct bt_conn *conn,
                                       struct bt_conn_le_data_len_info *info)
{
  UNUSED(conn);
  UNUSED(info);
}
#endif

static struct bt_conn_cb g_vg_conn_callbacks =
{
  .connected = vg_ble_connected,
  .disconnected = vg_ble_disconnected,
  .recycled = vg_ble_recycled,
  .le_param_req = vg_ble_le_param_req,
  .le_param_updated = vg_ble_le_param_updated,
  .security_changed = vg_ble_security_changed,
#if defined(CONFIG_BT_USER_PHY_UPDATE)
  .le_phy_updated = vg_ble_le_phy_updated,
#endif
#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
  .le_data_len_updated = vg_ble_le_data_len_updated,
#endif
};

static void vg_ble_pairing_complete(struct bt_conn *conn, bool bonded)
{
  UNUSED(conn);
  printf("VelaGuard BLE: pairing complete bonded=%d\n", bonded);
}

static void vg_ble_pairing_failed(struct bt_conn *conn,
                                  enum bt_security_err reason)
{
  UNUSED(conn);
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

static const struct bt_le_adv_param g_vg_adv_param =
  BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_USE_IDENTITY,
                       BT_GAP_ADV_FAST_INT_MIN_1,
                       BT_GAP_ADV_FAST_INT_MAX_1, NULL);

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

static bool vg_ble_addr_is_known_default(const char *addr)
{
  return strcmp(addr, "CD:AB:78:56:34:12") == 0 ||
         strcmp(addr, "12:34:56:78:AB:CD") == 0;
}

static void vg_ble_drop_connection_ref(void)
{
  if (g_vg_conn != NULL)
    {
      bt_conn_unref(g_vg_conn);
      g_vg_conn = NULL;
    }
}

static void vg_ble_cache_local_address(void)
{
  bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
  size_t count = ARRAY_SIZE(addrs);

  strlcpy(g_vg_local_addr, "pending", sizeof(g_vg_local_addr));
  g_vg_local_addr_default = false;

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
  g_vg_local_addr_default = vg_ble_addr_is_known_default(g_vg_local_addr);
  printf("VelaGuard BLE: local identity address %s%s\n", g_vg_local_addr,
         g_vg_local_addr_default ? " (default/test)" : "");
}

static void vg_ble_addr_from_hash(bt_addr_le_t *addr, uint64_t hash)
{
  int i;

  memset(addr, 0, sizeof(*addr));
  addr->type = BT_ADDR_LE_RANDOM;

  for (i = 0; i < 6; i++)
    {
      addr->a.val[i] = (hash >> (i * 8)) & 0xff;
    }

  /* Static random address: the two most significant bits must be 1. */

  addr->a.val[5] = (addr->a.val[5] & 0x3f) | 0xc0;
}

static bool vg_ble_read_factory_mac_seed(uint8_t *seed, size_t len)
{
  uint8_t mac[6];
  int ret;

  if (len < sizeof(mac))
    {
      return false;
    }

  memset(mac, 0, sizeof(mac));
  ret = BSP_CONFIG_get(VG_BLE_FACTORY_CFG_ID_MAC, mac, sizeof(mac));
  if (ret == sizeof(mac) && vg_ble_seed_is_valid(mac, sizeof(mac)))
    {
      memcpy(seed, mac, sizeof(mac));
      return true;
    }

  return false;
}

static bool vg_ble_read_efuse_seed(uint8_t *seed, size_t len)
{
  int32_t ret;

  if (len < VG_BLE_EFUSE_SEED_LEN)
    {
      return false;
    }

  memset(seed, 0, VG_BLE_EFUSE_SEED_LEN);
  ret = HAL_EFUSE_Init();
  if (ret != VG_BLE_HAL_OK)
    {
      printf("VelaGuard BLE: eFuse init failed (%ld)\n", (long)ret);
      return false;
    }

  ret = HAL_EFUSE_Read(VG_BLE_EFUSE_SEED_OFFSET, seed,
                       VG_BLE_EFUSE_SEED_LEN);
  if (ret < 0 || !vg_ble_seed_is_valid(seed, VG_BLE_EFUSE_SEED_LEN))
    {
      printf("VelaGuard BLE: eFuse seed unavailable ret=%ld\n", (long)ret);
      return false;
    }

  return true;
}

static int vg_ble_configure_identity(void)
{
  bt_addr_le_t addr;
  uint8_t seed[VG_BLE_EFUSE_SEED_LEN];
  char addr_str[BT_ADDR_LE_STR_LEN];
  uint64_t hash;
  const char *source = "fallback";
  int ret;

  if (g_vg_identity_configured)
    {
      return 0;
    }

  memset(seed, 0, sizeof(seed));
  if (vg_ble_read_factory_mac_seed(seed, sizeof(seed)))
    {
      source = "factory-mac";
    }
  else if (vg_ble_read_efuse_seed(seed, sizeof(seed)))
    {
      source = "efuse";
    }
  else
    {
      static const uint8_t fallback_seed[] =
      {
        'V', 'e', 'l', 'a', 'G', 'u', 'a', 'r', 'd', '-',
        'S', 'F', '3', '2', 'L', 'B', '5', '2'
      };

      memcpy(seed, fallback_seed, sizeof(fallback_seed));
    }

  hash = vg_ble_hash_seed(seed, sizeof(seed));
  vg_ble_addr_from_hash(&addr, hash);
  bt_addr_le_to_str(&addr, addr_str, sizeof(addr_str));

  ret = bt_id_create(&addr, NULL);
  if (ret < 0 && ret != -EALREADY)
    {
      printf("VelaGuard BLE: identity create failed addr=%s source=%s ret=%d\n",
             addr_str, source, ret);
      return ret;
    }

  g_vg_identity_configured = true;
  printf("VelaGuard BLE: identity configured addr=%s source=%s id=%d\n",
         addr_str, source, ret);
  return 0;
}

static int vg_ble_adv_start_raw(void)
{
  return bt_le_adv_start(&g_vg_adv_param,
                         g_vg_ad, ARRAY_SIZE(g_vg_ad),
                         g_vg_sd, ARRAY_SIZE(g_vg_sd));
}

static int vg_ble_start_advertising(void)
{
  int ret;

  ret = vg_ble_adv_start_raw();
  if (ret == -EALREADY)
    {
      g_vg_advertising = true;
      printf("VelaGuard BLE: advertising already active\n");
      return 0;
    }
  if (ret < 0)
    {
      g_vg_advertising = false;
      printf("VelaGuard BLE: advertising failed (%d)\n", ret);
      return ret;
    }

  g_vg_advertising = true;
  printf("VelaGuard BLE: advertising as VelaGuard\n");
  return 0;
}

static int vg_ble_restart_advertising(void)
{
  int ret;

  ret = vg_ble_adv_start_raw();
  if (ret == 0)
    {
      g_vg_advertising = true;
      printf("VelaGuard BLE: advertising restarted\n");
      return 0;
    }

  if (ret != -EALREADY)
    {
      g_vg_advertising = false;
      printf("VelaGuard BLE: advertising restart failed (%d)\n", ret);
      return ret;
    }

  g_vg_advertising = true;
  printf("VelaGuard BLE: advertising already active on restart\n");
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
  printf("VelaGuard BLE: init start\n");

  ret = vg_ble_configure_identity();
  if (ret < 0)
    {
      return ret;
    }

  ret = bt_enable(NULL);
  if (ret < 0 && ret != -EALREADY)
    {
      printf("VelaGuard BLE: stack init failed (%d)\n", ret);
      return ret;
    }

  printf("VelaGuard BLE: stack enabled ret=%d\n", ret);

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
      g_vg_restart_advertising = true;
      g_vg_adv_retry_skip = 1500;
      printf("VelaGuard BLE: init complete but advertising will retry (%d)\n",
             ret);
    }

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

  /* A user may press SOS while Android is still discovering services and
   * writing the CCC descriptor.  Keep the newest emergency packet queued and
   * deliver it as soon as the connected phone has enabled notifications.
   */

  if (g_vg_call_pending && g_vg_connected && g_vg_notify_enabled)
    {
      struct vg_ble_call_packet_s packet;
      irqstate_t irqstate;

      irqstate = enter_critical_section();
      packet = g_vg_last_packet;
      leave_critical_section(irqstate);

      ret = bt_gatt_notify(NULL,
                           &g_vg_service.attrs[VG_BLE_ATTR_EVENT_VALUE],
                           &packet, sizeof(packet));
      printf("VelaGuard BLE: CALL_REQUEST id=%lu result=%d\n",
             (unsigned long)packet.event_id, ret);
      if (ret == 0)
        {
          irqstate = enter_critical_section();
          g_vg_call_pending = false;
          leave_critical_section(irqstate);
        }
    }

  if (g_vg_connected && g_vg_test_notify_enabled && g_vg_test_probe_seen)
    {
      uint64_t now_ms = vg_ble_monotonic_ms();

      if (g_vg_last_test_notify_ms == 0 ||
          now_ms - g_vg_last_test_notify_ms >= VG_BLE_TEST_NOTIFY_MS)
        {
          uint8_t payload[4];

          g_vg_test_counter++;
          vg_ble_put_le32(payload, g_vg_test_counter);
          ret = bt_gatt_notify(NULL,
                               &g_vg_service.attrs[VG_BLE_ATTR_TEST_VALUE],
                               payload, sizeof(payload));
          printf("VelaGuard BLE TEST: notify counter=%lu result=%d\n",
                 (unsigned long)g_vg_test_counter, ret);
          if (ret == 0)
            {
              g_vg_last_test_notify_ms = now_ms;
            }
        }
    }

  if (!g_vg_restart_advertising || g_vg_connected)
    {
      return;
    }

  if (g_vg_adv_retry_skip > 0)
    {
      g_vg_adv_retry_skip--;
      return;
    }

  g_vg_restart_advertising = false;
  ret = vg_ble_restart_advertising();
  printf("VelaGuard BLE: advertising restart ret=%d\n", ret);
  if (ret < 0)
    {
      g_vg_restart_advertising = true;
      g_vg_adv_retry_skip = 250;
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

bool vg_ble_is_advertising(void)
{
  return g_vg_advertising;
}

bool vg_ble_has_pending_enable_request(void)
{
  return g_vg_enable_request_pending;
}

int vg_ble_set_enabled(bool enabled)
{
  int first_ret = 0;
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
  g_vg_advertising = false;

  if (g_vg_connected && g_vg_conn != NULL)
    {
      ret = bt_conn_disconnect(g_vg_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
      if (ret < 0)
        {
          printf("VelaGuard BLE: disconnect failed (%d)\n", ret);
          first_ret = ret;
        }
    }

  ret = bt_le_adv_stop();
  if (ret < 0 && ret != -EALREADY)
    {
      printf("VelaGuard BLE: advertising stop failed (%d)\n", ret);
      if (first_ret == 0)
        {
          first_ret = ret;
        }
    }

  printf("VelaGuard BLE: disabled ret=%d\n", first_ret);
  return first_ret;
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

  /* Do not issue a synchronous GATT/HCI operation from an LVGL callback.
   * vg_ble_process() sends this queued packet after the UI handler returns.
   */

  printf("VelaGuard BLE: CALL_REQUEST id=%lu queued\n",
         (unsigned long)event_id);
  return 0;
}
