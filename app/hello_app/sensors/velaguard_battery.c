/****************************************************************************
 * VelaGuard battery monitor task.
 *
 * The task owns ADC trigger/read operations so a slow VBAT conversion cannot
 * delay LVGL timers or rendering.
 ****************************************************************************/

#include "velaguard_battery.h"

#include "sifli_gpio.h"

#include <nuttx/analog/adc.h>
#include <nuttx/analog/ioctl.h>
#include <nuttx/sched.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define VG_BATTERY_ADC_DEVPATH       "/dev/adc0"
#define VG_BATTERY_VBUS_DET_PIN       GET_PIN_2(hwp_gpio1, 44)
#define VG_BATTERY_POLL_US            2000000
#define VG_BATTERY_FULL_MV            4200
#define VG_BATTERY_EMPTY_MV           3400
#define VG_BATTERY_ABSENT_MV          1000
#define VG_BATTERY_PCT_STEP           20

static struct vg_battery_status_s g_vg_battery;
static pthread_mutex_t g_vg_battery_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_vg_battery_started;

static int vg_battery_read_mv(int fd)
{
  struct adc_msg_s msg;
  ssize_t nread;
  if (ioctl(fd, ANIOC_TRIGGER, 0) < 0)
    {
      return -errno;
    }

  nread = read(fd, &msg, sizeof(msg));

  if (nread != (ssize_t)sizeof(msg))
    {
      return -EIO;
    }

  return (int)msg.am_data;
}

static int vg_battery_percentage(int millivolts)
{
  int percentage;

  if (millivolts >= VG_BATTERY_FULL_MV)
    {
      percentage = 100;
    }
  else if (millivolts <= VG_BATTERY_EMPTY_MV)
    {
      percentage = 0;
    }
  else
    {
      percentage = (millivolts - VG_BATTERY_EMPTY_MV) * 100 /
                   (VG_BATTERY_FULL_MV - VG_BATTERY_EMPTY_MV);
    }

  return ((percentage + VG_BATTERY_PCT_STEP / 2) / VG_BATTERY_PCT_STEP) *
         VG_BATTERY_PCT_STEP;
}

static void vg_battery_publish(int millivolts, int error, bool charging)
{
  pthread_mutex_lock(&g_vg_battery_lock);
  g_vg_battery.charging = charging;
  g_vg_battery.last_error = error;
  g_vg_battery.ready = error == 0;
  if (error == 0)
    {
      g_vg_battery.millivolts = millivolts;
      g_vg_battery.percentage = millivolts < VG_BATTERY_ABSENT_MV ?
                                -1 : vg_battery_percentage(millivolts);
    }

  g_vg_battery.sequence++;
  pthread_mutex_unlock(&g_vg_battery_lock);
}

static int vg_battery_task(int argc, char *argv[])
{
  int fd = -1;

  UNUSED(argc);
  UNUSED(argv);

  printf("VelaGuard battery: task started\n");
  for (; ; )
    {
      int millivolts;
      int error = 0;
      bool charging = sifli_gpio_read(VG_BATTERY_VBUS_DET_PIN);

      if (fd < 0)
        {
          fd = open(VG_BATTERY_ADC_DEVPATH, O_RDONLY);
          if (fd < 0)
            {
              error = -errno;
              printf("VelaGuard battery: open %s failed: %d\n",
                     VG_BATTERY_ADC_DEVPATH, errno);
            }
        }

      millivolts = error == 0 ? vg_battery_read_mv(fd) : error;
      if (millivolts < 0)
        {
          error = millivolts;
          if (fd >= 0)
            {
              close(fd);
              fd = -1;
            }
        }

      vg_battery_publish(millivolts, error, charging);
      usleep(VG_BATTERY_POLL_US);
    }

  return 0;
}

int vg_battery_task_start(int priority, int stacksize)
{
  int pid;

  pthread_mutex_lock(&g_vg_battery_lock);
  if (g_vg_battery_started)
    {
      pthread_mutex_unlock(&g_vg_battery_lock);
      return 0;
    }

  memset(&g_vg_battery, 0, sizeof(g_vg_battery));
  g_vg_battery.percentage = -1;
  g_vg_battery.last_error = -EINPROGRESS;
  g_vg_battery_started = true;
  pthread_mutex_unlock(&g_vg_battery_lock);

  pid = task_create("vg_battery", priority, stacksize, vg_battery_task,
                    NULL);
  if (pid < 0)
    {
      pthread_mutex_lock(&g_vg_battery_lock);
      g_vg_battery_started = false;
      g_vg_battery.last_error = pid;
      pthread_mutex_unlock(&g_vg_battery_lock);
      return pid;
    }

  printf("VelaGuard battery: task created pid=%d\n", pid);
  return 0;
}

void vg_battery_task_get_status(struct vg_battery_status_s *status)
{
  if (status == NULL)
    {
      return;
    }

  pthread_mutex_lock(&g_vg_battery_lock);
  *status = g_vg_battery;
  pthread_mutex_unlock(&g_vg_battery_lock);
}
