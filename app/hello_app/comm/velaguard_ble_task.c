/****************************************************************************
 * VelaGuard BLE task.
 ****************************************************************************/

#include "velaguard_ble_task.h"
#include "velaguard_ble.h"

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define VG_BLE_TASK_STACKSIZE 16384
#define VG_BLE_TASK_POLL_MS 10
#define VG_BLE_CMD_QNAME "/vg_ble_cmd"
#define VG_BLE_CMD_MAXMSG 8

enum vg_ble_cmd_type_e
{
  VG_BLE_CMD_ENABLE = 1,
  VG_BLE_CMD_CALL,
};

struct vg_ble_cmd_s
{
  uint32_t type;
  bool enabled;
  struct vg_ble_call_packet_s packet;
};

static bool g_vg_ble_task_started;
static volatile bool g_vg_ble_enable_pending;
static mqd_t g_vg_ble_cmd_send = -1;
static mqd_t g_vg_ble_cmd_recv = -1;

static void vg_ble_task_process_commands(void)
{
  struct vg_ble_cmd_s cmd;
  ssize_t received;

  for (; ; )
    {
      received = mq_receive(g_vg_ble_cmd_recv, (FAR char *)&cmd,
                            sizeof(cmd), NULL);
      if (received != (ssize_t)sizeof(cmd))
        {
          return;
        }

      if (cmd.type == VG_BLE_CMD_ENABLE)
        {
          g_vg_ble_enable_pending = false;
          vg_ble_process_enable_command(cmd.enabled);
        }
      else if (cmd.type == VG_BLE_CMD_CALL)
        {
          vg_ble_process_call_command(&cmd.packet);
        }
    }
}

static void *vg_ble_task_entry(void *arg)
{
  (void)arg;

  printf("VelaGuard BLE: task started\n");

  g_vg_ble_cmd_recv = mq_open(VG_BLE_CMD_QNAME, O_RDONLY | O_NONBLOCK);
  if (g_vg_ble_cmd_recv < 0)
    {
      printf("VelaGuard BLE: command queue open failed (%d)\n", errno);
      return NULL;
    }

  /* The Bluetooth Framework binds libuv and GATT objects to the task that
   * creates them.  Keep initialization in this owner task as well as every
   * later Framework call made by vg_ble_process(). */
  if (vg_ble_framework_init() < 0)
    {
      printf("VelaGuard BLE: framework init failed in task\n");
      return NULL;
    }

  for (; ; )
    {
      /* All periodic advertising, heartbeat and queued event notifications
       * are serialized here, outside the LVGL/main task. */
      vg_ble_task_process_commands();
      vg_ble_process();
      usleep(VG_BLE_TASK_POLL_MS * 1000);
    }

  return NULL;
}

int vg_ble_task_start(void)
{
  struct mq_attr attr;
  pthread_t thread;
  pthread_attr_t thread_attr;
  int ret;

  if (g_vg_ble_task_started)
    {
      return 0;
    }

  memset(&attr, 0, sizeof(attr));
  attr.mq_maxmsg = VG_BLE_CMD_MAXMSG;
  attr.mq_msgsize = sizeof(struct vg_ble_cmd_s);
  g_vg_ble_cmd_send = mq_open(VG_BLE_CMD_QNAME,
                              O_WRONLY | O_CREAT | O_NONBLOCK,
                              0666, &attr);
  if (g_vg_ble_cmd_send < 0)
    {
      printf("VelaGuard BLE: command queue create failed (%d)\n", errno);
      return -errno;
    }

  ret = pthread_attr_init(&thread_attr);
  if (ret != 0)
    {
      mq_close(g_vg_ble_cmd_send);
      g_vg_ble_cmd_send = -1;
      return -ret;
    }

  pthread_attr_setstacksize(&thread_attr, VG_BLE_TASK_STACKSIZE);
  ret = pthread_create(&thread, &thread_attr, vg_ble_task_entry, NULL);
  pthread_attr_destroy(&thread_attr);
  if (ret != 0)
    {
      printf("VelaGuard BLE: pthread create failed (%d)\n", ret);
      mq_close(g_vg_ble_cmd_send);
      g_vg_ble_cmd_send = -1;
      return -ret;
    }

  pthread_detach(thread);

  g_vg_ble_task_started = true;
  printf("VelaGuard BLE: pthread created\n");
  return 0;
}

int vg_ble_task_send_enable(bool enabled)
{
  struct vg_ble_cmd_s cmd;

  if (g_vg_ble_cmd_send < 0)
    {
      return -ENODEV;
    }

  memset(&cmd, 0, sizeof(cmd));
  cmd.type = VG_BLE_CMD_ENABLE;
  cmd.enabled = enabled;
  g_vg_ble_enable_pending = true;
  if (mq_send(g_vg_ble_cmd_send, (FAR char *)&cmd, sizeof(cmd), 0) < 0)
    {
      g_vg_ble_enable_pending = false;
      return -errno;
    }

  return 0;
}

int vg_ble_task_send_call(const struct vg_ble_call_packet_s *packet)
{
  struct vg_ble_cmd_s cmd;

  if (g_vg_ble_cmd_send < 0 || packet == NULL)
    {
      return -ENODEV;
    }

  memset(&cmd, 0, sizeof(cmd));
  cmd.type = VG_BLE_CMD_CALL;
  cmd.packet = *packet;
  if (mq_send(g_vg_ble_cmd_send, (FAR char *)&cmd, sizeof(cmd), 0) < 0)
    {
      return -errno;
    }

  return 0;
}

bool vg_ble_task_has_pending_enable(void)
{
  return g_vg_ble_enable_pending;
}
