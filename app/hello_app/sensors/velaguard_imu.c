/****************************************************************************
 * contest2026_148_langyongyunji/app/hello_app/sensors/velaguard_imu.c
 *
 * Minimal LSM6DS3TR-C reader for Huangshan Pi.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "velaguard_imu.h"

#include "velaguard_fall.h"

#include "bf0_hal_pinmux.h"

#include <nuttx/i2c/i2c_master.h>
#include <nuttx/sched.h>
#include <nuttx/sensors/lsm6dsl.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VG_IMU_I2C_FREQ       400000
#define LSM6DS3_ADDR_LOW      0x6a
#define LSM6DS3_ADDR_HIGH     0x6b
#define LSM6DS3_WHO_AM_I      0x0f
#define LSM6DS3_WHO_AM_I_VAL  0x69
#define LSM6DSL_WHO_AM_I_VAL  0x6a
#define LSM6DS3_FIFO_CTRL1    0x06
#define LSM6DS3_FIFO_CTRL2    0x07
#define LSM6DS3_FIFO_CTRL3    0x08
#define LSM6DS3_FIFO_CTRL5    0x0a
#define LSM6DS3_CTRL1_XL      0x10
#define LSM6DS3_CTRL2_G       0x11
#define LSM6DS3_CTRL3_C       0x12
#define LSM6DS3_OUTX_L_G      0x22
#define LSM6DS3_FIFO_STATUS1  0x3a
#define LSM6DS3_FIFO_DATA_OUT 0x3e

#define VG_IMU_FALL_PERIOD_MS 100
#define VG_IMU_GUARD_SAMPLE_US 200000
#define VG_IMU_FIFO_SAMPLE_MS  10
#define VG_IMU_FIFO_WORDS_PER_SAMPLE 6
#define VG_IMU_FIFO_BYTES_PER_SAMPLE 12
#define VG_IMU_FIFO_MAX_SAMPLES 24
#define VG_IMU_PIN_SETTLE_US  500
#define VG_IMU_GUARD_DEVPATH_MAX 32

#ifndef CONFIG_CONTEST2026_148_VELAGUARD_IMU_DEVPATH
#  define CONFIG_CONTEST2026_148_VELAGUARD_IMU_DEVPATH "/dev/lsm6dsl0"
#endif

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

extern void BSP_GPIO_Set(int pin, int val, int is_porta);
extern void BSP_PIN_Touch(void);

static int vg_imu_read_fifo_guarded(struct vg_imu_s *imu,
                                    struct vg_imu_sample_s *samples,
                                    int max_samples);

#ifdef CONFIG_CONTEST2026_148_VELAGUARD_IMU_LEGACY_I2C
static pthread_mutex_t g_sf32lb52_i2c1_mux_lock = PTHREAD_MUTEX_INITIALIZER;

int sf32lb52_i2c1_mux_lock(void)
{
  return pthread_mutex_lock(&g_sf32lb52_i2c1_mux_lock);
}

int sf32lb52_i2c1_mux_unlock(void)
{
  return pthread_mutex_unlock(&g_sf32lb52_i2c1_mux_lock);
}
#else
int sf32lb52_i2c1_mux_lock(void)
{
  /* No-op: the G-SENSOR is on a dedicated I2C3 bus, so there is no
   * shared-I2C1 contention with the touch controller anymore.
   */

  return 0;
}

int sf32lb52_i2c1_mux_unlock(void)
{
  return 0;
}
#endif

struct vg_imu_guard_s
{
  struct vg_imu_guard_status_s status;
  struct vg_fall_detector_s detector;
  struct vg_fall_result_s pending_result;
  char devpath[VG_IMU_GUARD_DEVPATH_MAX];
  bool enabled;
  bool result_pending;
  bool started;
};

static struct vg_imu_guard_s g_vg_imu_guard;
static pthread_mutex_t g_vg_imu_guard_lock = PTHREAD_MUTEX_INITIALIZER;

static void vg_imu_guard_lock(void)
{
  pthread_mutex_lock(&g_vg_imu_guard_lock);
}

static void vg_imu_guard_unlock(void)
{
  pthread_mutex_unlock(&g_vg_imu_guard_lock);
}

static bool vg_imu_guard_is_enabled(void)
{
  bool enabled;

  vg_imu_guard_lock();
  enabled = g_vg_imu_guard.enabled;
  vg_imu_guard_unlock();
  return enabled;
}

static bool vg_imu_guard_has_pending_result(void)
{
  bool pending;

  vg_imu_guard_lock();
  pending = g_vg_imu_guard.result_pending;
  vg_imu_guard_unlock();
  return pending;
}

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint32_t vg_imu_uptime_ms(void)
{
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
    {
      return 0;
    }

  return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static uint64_t vg_imu_u64_abs(int64_t value)
{
  return value < 0 ? (uint64_t)-value : (uint64_t)value;
}

static uint32_t vg_imu_isqrt64(uint64_t value)
{
  uint64_t bit = (uint64_t)1 << 62;
  uint64_t result = 0;

  while (bit > value)
    {
      bit >>= 2;
    }

  while (bit != 0)
    {
      if (value >= result + bit)
        {
          value -= result + bit;
          result = (result >> 1) + bit;
        }
      else
        {
          result >>= 1;
        }

      bit >>= 2;
    }

  return (uint32_t)result;
}

static int vg_imu_accel_mag_mg(const struct vg_imu_sample_s *sample)
{
  uint64_t x = vg_imu_u64_abs(sample->ax_mg);
  uint64_t y = vg_imu_u64_abs(sample->ay_mg);
  uint64_t z = vg_imu_u64_abs(sample->az_mg);

  return (int)vg_imu_isqrt64(x * x + y * y + z * z);
}

static int vg_imu_gyro_sum_dps(const struct vg_imu_sample_s *sample)
{
  uint64_t x = vg_imu_u64_abs(sample->gx_dps);
  uint64_t y = vg_imu_u64_abs(sample->gy_dps);
  uint64_t z = vg_imu_u64_abs(sample->gz_dps);

  return (int)vg_imu_isqrt64(x * x + y * y + z * z);
}

static void vg_imu_to_fall_sample(const struct vg_imu_sample_s *imu,
                                  struct vg_fall_sample_s *fall)
{
  fall->timestamp_ms = imu->timestamp_ms;
  fall->ax_mg = imu->ax_mg;
  fall->ay_mg = imu->ay_mg;
  fall->az_mg = imu->az_mg;
  fall->gx_dps = imu->gx_dps;
  fall->gy_dps = imu->gy_dps;
  fall->gz_dps = imu->gz_dps;
}

static void vg_imu_guard_reset_detector(void)
{
  vg_fall_init(&g_vg_imu_guard.detector);

  vg_imu_guard_lock();
  g_vg_imu_guard.status.detector_state = 0;
  g_vg_imu_guard.status.peak_mg = 0;
  g_vg_imu_guard.status.peak_gyro_dps = 0;
  g_vg_imu_guard.status.posture_delta_deg = 0;
  g_vg_imu_guard.status.still_ms = 0;
  vg_imu_guard_unlock();
}

static void vg_imu_guard_set_error(int error)
{
  vg_imu_guard_lock();
  g_vg_imu_guard.status.ready = false;
  g_vg_imu_guard.status.last_error = error;
  vg_imu_guard_unlock();
}

static void vg_imu_guard_set_ready(void)
{
  vg_imu_guard_lock();
  g_vg_imu_guard.status.ready = true;
  g_vg_imu_guard.status.last_error = 0;
  vg_imu_guard_unlock();
}

static void vg_imu_guard_update_sample(const struct vg_fall_sample_s *sample)
{
  vg_imu_guard_lock();
  g_vg_imu_guard.status.mag_mg = vg_fall_accel_mag_mg(sample);
  g_vg_imu_guard.status.gyro_dps = vg_fall_gyro_sum_dps(sample);
  vg_imu_guard_unlock();
}

static void vg_imu_guard_update_detector(void)
{
  vg_imu_guard_lock();
  g_vg_imu_guard.status.detector_state = g_vg_imu_guard.detector.state;
  g_vg_imu_guard.status.peak_mg = g_vg_imu_guard.detector.peak_mg;
  g_vg_imu_guard.status.peak_gyro_dps =
      g_vg_imu_guard.detector.peak_gyro_dps;
  g_vg_imu_guard.status.posture_delta_deg =
      g_vg_imu_guard.detector.posture_delta_deg;
  g_vg_imu_guard.status.still_ms = g_vg_imu_guard.detector.still_ms;
  vg_imu_guard_unlock();
}

static void vg_imu_guard_queue_fall_result(
  const struct vg_fall_result_s *result)
{
  vg_imu_guard_lock();
  if (!g_vg_imu_guard.result_pending)
    {
      g_vg_imu_guard.pending_result = *result;
      g_vg_imu_guard.result_pending = true;
    }
  vg_imu_guard_unlock();
}

static int vg_imu_guard_task(int argc, char *argv[])
{
  struct vg_imu_s imu;
  struct vg_imu_sample_s imu_samples[VG_IMU_FIFO_MAX_SAMPLES];
  struct vg_fall_sample_s fall_sample;
  struct vg_fall_result_s fall_result;
  int nsamples;
  int i;
  int ret;

  UNUSED(argc);
  UNUSED(argv);

  for (; ; )
    {
      ret = vg_imu_open_guarded(&imu, g_vg_imu_guard.devpath);
      if (ret >= 0)
        {
          break;
        }

      vg_imu_guard_set_error(ret);
      printf("VelaGuard IMU task: open/probe failed: %d\n", ret);
      usleep(500000);
    }

  vg_imu_guard_set_ready();
  printf("VelaGuard IMU task: addr=0x%02x whoami=0x%02x\n",
         imu.addr, imu.whoami);

  for (; ; )
    {
      if (!vg_imu_guard_is_enabled())
        {
          vg_imu_guard_reset_detector();
          usleep(100000);
          continue;
        }

      if (vg_imu_guard_has_pending_result())
        {
          usleep(VG_IMU_GUARD_SAMPLE_US);
          continue;
        }

      ret = vg_imu_read_fifo_guarded(&imu, imu_samples,
                                     VG_IMU_FIFO_MAX_SAMPLES);
      if (ret < 0)
        {
          vg_imu_guard_set_error(ret);
          printf("VelaGuard IMU task: FIFO read failed: %d\n", ret);
          usleep(100000);
          continue;
        }

      nsamples = ret;
      if (nsamples == 0)
        {
          usleep(VG_IMU_GUARD_SAMPLE_US);
          continue;
        }

      vg_imu_guard_set_ready();

      for (i = 0; i < nsamples; i++)
        {
          vg_imu_to_fall_sample(&imu_samples[i], &fall_sample);
          vg_imu_guard_update_sample(&fall_sample);

          if (vg_fall_process(&g_vg_imu_guard.detector, &fall_sample,
                              &fall_result))
            {
              vg_imu_guard_update_detector();
              printf("VelaGuard IMU task: fall detected %s\n",
                     fall_result.reason);
              vg_imu_guard_queue_fall_result(&fall_result);
              break;
            }

          vg_imu_guard_update_detector();
        }

      usleep(VG_IMU_GUARD_SAMPLE_US);
    }

  return 0;
}

#ifdef CONFIG_CONTEST2026_148_VELAGUARD_IMU_LEGACY_I2C
static void vg_imu_select_pins(void)
{
  /* Huangshan Pi shares I2C1 controller between touch and IMU pin groups.
   * Select the IMU group only while doing explicit IMU access.
   */

  BSP_GPIO_Set(30, 1, 1);
  usleep(VG_IMU_PIN_SETTLE_US);
  HAL_PIN_Set(PAD_PA40, I2C1_SCL, PIN_PULLUP, 1);
  HAL_PIN_Set(PAD_PA39, I2C1_SDA, PIN_PULLUP, 1);
  usleep(VG_IMU_PIN_SETTLE_US);
}

static void vg_imu_restore_touch_pins(void)
{
  BSP_PIN_Touch();
  usleep(VG_IMU_PIN_SETTLE_US);
}
#else
static void vg_imu_select_pins(void)
{
  /* No-op: the G-SENSOR is wired to a dedicated I2C3 bus, so there is no
   * pin mux to switch.  Kept so higher-level callers stay unchanged.
   */
}

static void vg_imu_restore_touch_pins(void)
{
  /* No-op. */
}
#endif

#ifdef CONFIG_CONTEST2026_148_VELAGUARD_IMU_LEGACY_I2C
static int16_t vg_i16_le(const uint8_t *data)
{
  return (int16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static int vg_i2c_transfer(int fd, struct i2c_msg_s *msgs, int msgc)
{
  struct i2c_transfer_s transfer;

  memset(&transfer, 0, sizeof(transfer));
  transfer.msgv = msgs;
  transfer.msgc = msgc;

  return ioctl(fd, I2CIOC_TRANSFER, (unsigned long)(uintptr_t)&transfer);
}

static int vg_imu_read_reg(int fd, uint8_t addr, uint8_t reg,
                           uint8_t *buffer, uint16_t length)
{
  struct i2c_msg_s msgs[2];
  int ret;

  memset(msgs, 0, sizeof(msgs));

  msgs[0].frequency = VG_IMU_I2C_FREQ;
  msgs[0].addr      = addr;
  msgs[0].flags     = I2C_M_NOSTOP;
  msgs[0].buffer    = &reg;
  msgs[0].length    = 1;

  msgs[1].frequency = VG_IMU_I2C_FREQ;
  msgs[1].addr      = addr;
  msgs[1].flags     = I2C_M_READ | I2C_M_NOSTART;
  msgs[1].buffer    = buffer;
  msgs[1].length    = length;

  ret = vg_i2c_transfer(fd, msgs, 2);
  return ret < 0 ? ret : 0;
}

static int vg_imu_write_reg(int fd, uint8_t addr, uint8_t reg, uint8_t value)
{
  struct i2c_msg_s msg;
  uint8_t data[2];
  int ret;

  data[0] = reg;
  data[1] = value;

  memset(&msg, 0, sizeof(msg));
  msg.frequency = VG_IMU_I2C_FREQ;
  msg.addr      = addr;
  msg.flags     = 0;
  msg.buffer    = data;
  msg.length    = sizeof(data);

  ret = vg_i2c_transfer(fd, &msg, 1);
  return ret < 0 ? ret : 0;
}

static int vg_imu_probe_addr(int fd, uint8_t addr, uint8_t *whoami)
{
  int ret;

  ret = vg_imu_read_reg(fd, addr, LSM6DS3_WHO_AM_I, whoami, 1);
  if (ret < 0)
    {
      return ret;
    }

  if (*whoami == LSM6DS3_WHO_AM_I_VAL ||
      *whoami == LSM6DSL_WHO_AM_I_VAL)
    {
      return 0;
    }

  return -ENODEV;
}

static int vg_imu_configure(struct vg_imu_s *imu)
{
  int ret;

  ret = vg_imu_write_reg(imu->fd, imu->addr, LSM6DS3_FIFO_CTRL5, 0x00);
  if (ret < 0)
    {
      return ret;
    }

  /* BDU + auto-increment register address. */

  ret = vg_imu_write_reg(imu->fd, imu->addr, LSM6DS3_CTRL3_C, 0x44);
  if (ret < 0)
    {
      return ret;
    }

  /* Accelerometer: 104 Hz, +-8 g. The guard task samples every 100 ms to
   * reduce contention with the FT6146 touch controller on the shared I2C1
   * pinmux.
   */

  ret = vg_imu_write_reg(imu->fd, imu->addr, LSM6DS3_CTRL1_XL, 0x4c);
  if (ret < 0)
    {
      return ret;
    }

  /* Gyroscope: 104 Hz, 2000 dps. */

  ret = vg_imu_write_reg(imu->fd, imu->addr, LSM6DS3_CTRL2_G, 0x4c);
  if (ret < 0)
    {
      return ret;
    }

  ret = vg_imu_write_reg(imu->fd, imu->addr, LSM6DS3_FIFO_CTRL1, 0x00);
  if (ret < 0)
    {
      return ret;
    }

  ret = vg_imu_write_reg(imu->fd, imu->addr, LSM6DS3_FIFO_CTRL2, 0x00);
  if (ret < 0)
    {
      return ret;
    }

  /* Batch accelerometer and gyroscope at the sensor ODR, then let the guard
   * task drain FIFO in short windows so touch keeps ownership of I2C1 most of
   * the time.
   */

  ret = vg_imu_write_reg(imu->fd, imu->addr, LSM6DS3_FIFO_CTRL3, 0x09);
  if (ret < 0)
    {
      return ret;
    }

  ret = vg_imu_write_reg(imu->fd, imu->addr, LSM6DS3_FIFO_CTRL5, 0x26);
  if (ret < 0)
    {
      return ret;
    }

  usleep(30000);
  return 0;
}
#endif /* CONFIG_CONTEST2026_148_VELAGUARD_IMU_LEGACY_I2C */

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#ifdef CONFIG_CONTEST2026_148_VELAGUARD_IMU_LEGACY_I2C
int vg_imu_open(struct vg_imu_s *imu, const char *devpath)
{
  static const uint8_t addrs[] =
  {
    LSM6DS3_ADDR_LOW,
    LSM6DS3_ADDR_HIGH,
  };

  uint8_t whoami;
  int i;
  int ret;

  memset(imu, 0, sizeof(*imu));
  imu->fd = -1;

  imu->fd = open(devpath, O_RDWR | O_CLOEXEC);
  if (imu->fd < 0)
    {
      return -errno;
    }

  for (i = 0; i < (int)(sizeof(addrs) / sizeof(addrs[0])); i++)
    {
      ret = vg_imu_probe_addr(imu->fd, addrs[i], &whoami);
      if (ret == 0)
        {
          imu->addr = addrs[i];
          imu->whoami = whoami;
          ret = vg_imu_configure(imu);
          if (ret < 0)
            {
              vg_imu_close(imu);
              return ret;
            }

          return 0;
        }
    }

  vg_imu_close(imu);
  return -ENODEV;
}
#else
int vg_imu_open(struct vg_imu_s *imu, const char *devpath)
{
  int ret;

  memset(imu, 0, sizeof(*imu));
  imu->fd = -1;

  imu->fd = open(devpath, O_RDWR | O_CLOEXEC);
  if (imu->fd < 0)
    {
      return -errno;
    }

  /* The kernel LSM6DSL driver is already bound to the sensor's fixed I2C
   * address (0x6a/0x6b).  The driver only validates WHO_AM_I at register
   * time, so it must be explicitly started (SNIOC_START sets the accel and
   * gyro ODR) before the first sensor read; otherwise the sensor stays in
   * power-down and every read returns zeros.
   */

  ret = ioctl(imu->fd, SNIOC_START, 0);
  if (ret < 0)
    {
      ret = -errno;
      vg_imu_close(imu);
      return ret;
    }

  imu->addr   = LSM6DS3_ADDR_LOW;
  imu->whoami = LSM6DSL_WHO_AM_I_VAL;

  return 0;
}
#endif /* CONFIG_CONTEST2026_148_VELAGUARD_IMU_LEGACY_I2C */

int vg_imu_guard_start(const char *devpath, int priority, int stacksize)
{
  int pid;

  if (devpath == NULL)
    {
      return -EINVAL;
    }

  vg_imu_guard_lock();
  if (g_vg_imu_guard.started)
    {
      vg_imu_guard_unlock();
      return -EEXIST;
    }

  memset(&g_vg_imu_guard, 0, sizeof(g_vg_imu_guard));
  snprintf(g_vg_imu_guard.devpath, sizeof(g_vg_imu_guard.devpath),
           "%s", devpath);
  g_vg_imu_guard.status.last_error = -EINPROGRESS;
  vg_fall_init(&g_vg_imu_guard.detector);
  g_vg_imu_guard.started = true;
  vg_imu_guard_unlock();

  pid = task_create("vg_imu", priority, stacksize, vg_imu_guard_task, NULL);
  if (pid < 0)
    {
      vg_imu_guard_lock();
      g_vg_imu_guard.started = false;
      vg_imu_guard_unlock();
      return pid;
    }

  return 0;
}

void vg_imu_guard_set_enabled(bool enabled)
{
  vg_imu_guard_lock();
  g_vg_imu_guard.enabled = enabled;
  vg_imu_guard_unlock();
}

void vg_imu_guard_get_status(struct vg_imu_guard_status_s *status)
{
  if (status == NULL)
    {
      return;
    }

  vg_imu_guard_lock();
  *status = g_vg_imu_guard.status;
  vg_imu_guard_unlock();
}

bool vg_imu_guard_take_fall_result(struct vg_fall_result_s *result)
{
  bool have_result;

  if (result == NULL)
    {
      return false;
    }

  vg_imu_guard_lock();
  have_result = g_vg_imu_guard.result_pending;
  if (have_result)
    {
      *result = g_vg_imu_guard.pending_result;
      g_vg_imu_guard.result_pending = false;
    }
  vg_imu_guard_unlock();

  return have_result;
}

int vg_imu_open_guarded(struct vg_imu_s *imu, const char *devpath)
{
  int ret;

  ret = sf32lb52_i2c1_mux_lock();
  if (ret < 0)
    {
      return ret;
    }

  vg_imu_select_pins();
  ret = vg_imu_open(imu, devpath);
  vg_imu_restore_touch_pins();
  sf32lb52_i2c1_mux_unlock();
  return ret;
}

void vg_imu_close(struct vg_imu_s *imu)
{
  if (imu->fd >= 0)
    {
      close(imu->fd);
      imu->fd = -1;
    }
}

#ifdef CONFIG_CONTEST2026_148_VELAGUARD_IMU_LEGACY_I2C
static void vg_imu_decode_sample(const uint8_t *data, uint32_t timestamp_ms,
                                 struct vg_imu_sample_s *sample)
{
  int16_t gx;
  int16_t gy;
  int16_t gz;
  int16_t ax;
  int16_t ay;
  int16_t az;

  gx = vg_i16_le(&data[0]);
  gy = vg_i16_le(&data[2]);
  gz = vg_i16_le(&data[4]);
  ax = vg_i16_le(&data[6]);
  ay = vg_i16_le(&data[8]);
  az = vg_i16_le(&data[10]);

  sample->timestamp_ms = timestamp_ms;
  sample->ax_mg = ((int32_t)ax * 244 + (ax >= 0 ? 500 : -500)) / 1000;
  sample->ay_mg = ((int32_t)ay * 244 + (ay >= 0 ? 500 : -500)) / 1000;
  sample->az_mg = ((int32_t)az * 244 + (az >= 0 ? 500 : -500)) / 1000;
  sample->gx_dps = ((int32_t)gx * 70 + (gx >= 0 ? 500 : -500)) / 1000;
  sample->gy_dps = ((int32_t)gy * 70 + (gy >= 0 ? 500 : -500)) / 1000;
  sample->gz_dps = ((int32_t)gz * 70 + (gz >= 0 ? 500 : -500)) / 1000;
}
#endif /* CONFIG_CONTEST2026_148_VELAGUARD_IMU_LEGACY_I2C */

#ifdef CONFIG_CONTEST2026_148_VELAGUARD_IMU_LEGACY_I2C
int vg_imu_read(struct vg_imu_s *imu, struct vg_imu_sample_s *sample)
{
  uint8_t data[VG_IMU_FIFO_BYTES_PER_SAMPLE];
  int ret;

  ret = vg_imu_read_reg(imu->fd, imu->addr, LSM6DS3_OUTX_L_G,
                        data, sizeof(data));
  if (ret < 0)
    {
      return ret;
    }

  vg_imu_decode_sample(data, vg_imu_uptime_ms(), sample);

  return 0;
}
#else
int vg_imu_read(struct vg_imu_s *imu, struct vg_imu_sample_s *sample)
{
  struct lsm6dsl_sensor_data_s data;
  int ret;

  /* The kernel driver returns one sample per ioctl: acceleration in mg
   * and angular rate in mdps (milli-degrees per second).
   */

  ret = ioctl(imu->fd, SNIOC_LSM6DSLSENSORREAD,
              (unsigned long)(uintptr_t)&data);
  if (ret < 0)
    {
      return -errno;
    }

  sample->timestamp_ms = vg_imu_uptime_ms();
  sample->ax_mg = data.x_data;
  sample->ay_mg = data.y_data;
  sample->az_mg = data.z_data;
  sample->gx_dps = (data.g_x_data + (data.g_x_data >= 0 ? 500 : -500)) / 1000;
  sample->gy_dps = (data.g_y_data + (data.g_y_data >= 0 ? 500 : -500)) / 1000;
  sample->gz_dps = (data.g_z_data + (data.g_z_data >= 0 ? 500 : -500)) / 1000;

  return 0;
}
#endif /* CONFIG_CONTEST2026_148_VELAGUARD_IMU_LEGACY_I2C */

#ifdef CONFIG_CONTEST2026_148_VELAGUARD_IMU_LEGACY_I2C
static int vg_imu_read_fifo(struct vg_imu_s *imu,
                            struct vg_imu_sample_s *samples,
                            int max_samples)
{
  uint8_t status[2];
  uint8_t data[VG_IMU_FIFO_BYTES_PER_SAMPLE];
  uint8_t word[2];
  uint32_t now_ms;
  int words;
  int nsamples;
  int i;
  int j;
  int ret;

  if (max_samples > VG_IMU_FIFO_MAX_SAMPLES)
    {
      max_samples = VG_IMU_FIFO_MAX_SAMPLES;
    }

  ret = vg_imu_read_reg(imu->fd, imu->addr, LSM6DS3_FIFO_STATUS1,
                        status, sizeof(status));
  if (ret < 0)
    {
      return ret;
    }

  words = status[0] | ((status[1] & 0x0f) << 8);
  nsamples = words / VG_IMU_FIFO_WORDS_PER_SAMPLE;
  if (nsamples <= 0)
    {
      return 0;
    }

  if (nsamples > max_samples)
    {
      nsamples = max_samples;
    }

  now_ms = vg_imu_uptime_ms();
  for (i = 0; i < nsamples; i++)
    {
      uint32_t ts = now_ms -
        (uint32_t)(nsamples - i - 1) * VG_IMU_FIFO_SAMPLE_MS;

      for (j = 0; j < VG_IMU_FIFO_WORDS_PER_SAMPLE; j++)
        {
          ret = vg_imu_read_reg(imu->fd, imu->addr, LSM6DS3_FIFO_DATA_OUT,
                                word, sizeof(word));
          if (ret < 0)
            {
              return ret;
            }

          data[j * 2] = word[0];
          data[j * 2 + 1] = word[1];
        }

      vg_imu_decode_sample(data, ts, &samples[i]);
    }

  return nsamples;
}

static int vg_imu_read_fifo_guarded(struct vg_imu_s *imu,
                                    struct vg_imu_sample_s *samples,
                                    int max_samples)
{
  int ret;

  ret = sf32lb52_i2c1_mux_lock();
  if (ret < 0)
    {
      return ret;
    }

  vg_imu_select_pins();
  ret = vg_imu_read_fifo(imu, samples, max_samples);
  vg_imu_restore_touch_pins();
  sf32lb52_i2c1_mux_unlock();
  return ret;
}
#else
static int vg_imu_read_fifo_guarded(struct vg_imu_s *imu,
                                    struct vg_imu_sample_s *samples,
                                    int max_samples)
{
  int ret;

  /* On the dedicated I2C3 bus there is no pin-mux contention to batch
   * around, so simply read one fresh sample per call.
   */

  ret = vg_imu_read_guarded(imu, &samples[0]);
  if (ret < 0)
    {
      return ret;
    }

  return 1;
}
#endif /* CONFIG_CONTEST2026_148_VELAGUARD_IMU_LEGACY_I2C */

int vg_imu_read_guarded(struct vg_imu_s *imu,
                        struct vg_imu_sample_s *sample)
{
  int ret;

  ret = sf32lb52_i2c1_mux_lock();
  if (ret < 0)
    {
      return ret;
    }

  vg_imu_select_pins();
  ret = vg_imu_read(imu, sample);
  vg_imu_restore_touch_pins();
  sf32lb52_i2c1_mux_unlock();
  return ret;
}

int vg_imu_selftest(const char *devpath, int nsamples)
{
  struct vg_imu_s imu;
  struct vg_imu_sample_s sample;
  int i;
  int ret;

#ifdef CONFIG_CONTEST2026_148_VELAGUARD_IMU_LEGACY_I2C
  printf("VelaGuard IMU: switch I2C1 pins to IMU SCL=PA40 SDA=PA39 PWR=PA30\n");
#endif
  vg_imu_select_pins();

  ret = vg_imu_open(&imu, devpath);
  if (ret < 0)
    {
      printf("VelaGuard IMU: open/probe %s failed: %d\n", devpath, ret);
      vg_imu_restore_touch_pins();
      return ret;
    }

  printf("VelaGuard IMU: LSM6DSx compatible addr=0x%02x whoami=0x%02x\n",
         imu.addr, imu.whoami);

  for (i = 0; i < nsamples; i++)
    {
      ret = vg_imu_read(&imu, &sample);
      if (ret < 0)
        {
          printf("VelaGuard IMU: read failed: %d\n", ret);
          break;
        }

      printf("IMU %03d t=%" PRIu32
             " ax=%" PRId32 "mg ay=%" PRId32 "mg az=%" PRId32 "mg"
             " gx=%" PRId32 "dps gy=%" PRId32 "dps gz=%" PRId32 "dps\n",
             i + 1, sample.timestamp_ms,
             sample.ax_mg, sample.ay_mg, sample.az_mg,
             sample.gx_dps, sample.gy_dps, sample.gz_dps);
      usleep(100000);
    }

  vg_imu_close(&imu);
  vg_imu_restore_touch_pins();
  return ret < 0 ? ret : 0;
}

int vg_imu_fall_watch(const char *devpath, int seconds)
{
  struct vg_imu_s imu;
  struct vg_imu_sample_s sample;
  struct vg_fall_detector_s detector;
  struct vg_fall_sample_s fall_sample;
  struct vg_fall_result_s result;
  uint32_t last_print_ms = 0;
  int event_id = 2000;
  int last_state = 0;
  int loops;
  int i;
  int ret;

  if (seconds <= 0)
    {
      seconds = 30;
    }

  loops = seconds * 1000 / VG_IMU_FALL_PERIOD_MS;

  printf("VelaGuard Fall: watching real IMU for %d seconds, sample=%dms\n",
         seconds, VG_IMU_FALL_PERIOD_MS);
  printf("VelaGuard Fall: actions: keep still 2s, simulate fall on soft pad, then keep still 5s\n");
#ifdef CONFIG_CONTEST2026_148_VELAGUARD_IMU_LEGACY_I2C
  printf("VelaGuard IMU: switch I2C1 pins to IMU SCL=PA40 SDA=PA39 PWR=PA30\n");
#endif
  vg_imu_select_pins();

  ret = vg_imu_open(&imu, devpath);
  if (ret < 0)
    {
      printf("VelaGuard Fall: open/probe %s failed: %d\n", devpath, ret);
      vg_imu_restore_touch_pins();
      return ret;
    }

  printf("VelaGuard Fall: LSM6DSx addr=0x%02x whoami=0x%02x\n",
         imu.addr, imu.whoami);

  vg_fall_init(&detector);

  for (i = 0; i < loops; i++)
    {
      int mag_mg;
      int gyro_dps;

      ret = vg_imu_read(&imu, &sample);
      if (ret < 0)
        {
          printf("VelaGuard Fall: IMU read failed: %d\n", ret);
          break;
        }

      mag_mg = vg_imu_accel_mag_mg(&sample);
      gyro_dps = vg_imu_gyro_sum_dps(&sample);
      vg_imu_to_fall_sample(&sample, &fall_sample);

      if (vg_fall_process(&detector, &fall_sample, &result))
        {
          event_id++;
          printf("VelaGuard Fall: DETECTED confidence=%d risk=%d %s\n",
                 result.confidence, result.risk, result.reason);
          printf("VELAGUARD_EVENT "
                 "{\"app\":\"VelaGuard\",\"id\":%d,"
                 "\"phase\":\"suspected\",\"type\":\"fall_suspected\","
                 "\"uptime_ms\":%" PRIu32
                 ",\"risk\":%d,\"confidence\":%d,"
                 "\"summary\":\"真实IMU：峰值%dmg，姿态变化%d度，静止%dms，判定疑似跌倒。\"}\n",
                 event_id, sample.timestamp_ms, result.risk,
                 result.confidence, result.peak_mg,
                 result.posture_delta_deg, result.still_ms);
          fflush(stdout);
        }

      if (detector.state != last_state)
        {
          if (detector.state != 0)
            {
              printf("VelaGuard Fall: candidate impact mag=%dmg gyro=%ddps peak=%dmg\n",
                     mag_mg, gyro_dps, detector.peak_mg);
            }
          else
            {
              printf("VelaGuard Fall: candidate timeout/reset\n");
            }

          last_state = detector.state;
        }

      if (last_print_ms == 0 ||
          sample.timestamp_ms - last_print_ms >= 1000)
        {
          printf("FALL_MONITOR t=%" PRIu32
                 " mag=%dmg gyro=%ddps state=%d peak=%dmg posture=%ddeg still=%dms\n",
                 sample.timestamp_ms, mag_mg, gyro_dps, detector.state,
                 detector.peak_mg, detector.posture_delta_deg,
                 detector.still_ms);
          last_print_ms = sample.timestamp_ms;
        }

      usleep(VG_IMU_FALL_PERIOD_MS * 1000);
    }

  vg_imu_close(&imu);
  vg_imu_restore_touch_pins();

  return ret < 0 ? ret : 0;
}

#ifdef CONFIG_CONTEST2026_148_VELAGUARD_IMU_LEGACY_I2C
int vg_imu_scan(void)
{
  static const char *devpaths[] =
  {
    "/dev/i2c0",
    "/dev/i2c1",
  };
  static const uint8_t addrs[] =
  {
    LSM6DS3_ADDR_LOW,
    LSM6DS3_ADDR_HIGH,
  };

  uint8_t whoami;
  bool found = false;
  int fd;
  int i;
  int j;
  int ret;

  printf("VelaGuard IMU: switch I2C1 pins to IMU SCL=PA40 SDA=PA39 PWR=PA30\n");
  vg_imu_select_pins();

  for (i = 0; i < (int)(sizeof(devpaths) / sizeof(devpaths[0])); i++)
    {
      fd = open(devpaths[i], O_RDWR | O_CLOEXEC);
      if (fd < 0)
        {
          printf("VelaGuard IMU: %s open failed: %d\n",
                 devpaths[i], -errno);
          continue;
        }

      for (j = 0; j < (int)(sizeof(addrs) / sizeof(addrs[0])); j++)
        {
          whoami = 0xff;
          ret = vg_imu_read_reg(fd, addrs[j], LSM6DS3_WHO_AM_I,
                                &whoami, 1);
          if (ret < 0)
            {
              printf("VelaGuard IMU: %s addr=0x%02x WHO_AM_I read failed: %d\n",
                     devpaths[i], addrs[j], ret);
            }
          else
            {
              printf("VelaGuard IMU: %s addr=0x%02x WHO_AM_I=0x%02x%s\n",
                     devpaths[i], addrs[j], whoami,
                     (whoami == LSM6DS3_WHO_AM_I_VAL ||
                      whoami == LSM6DSL_WHO_AM_I_VAL) ? " OK" : "");
              found = found || whoami == LSM6DS3_WHO_AM_I_VAL ||
                      whoami == LSM6DSL_WHO_AM_I_VAL;
            }
        }

      close(fd);
    }

  vg_imu_restore_touch_pins();
  return found ? 0 : -ENODEV;
}
#else
int vg_imu_scan(void)
{
  struct vg_imu_s imu;
  int ret;

  ret = vg_imu_open(&imu, CONFIG_CONTEST2026_148_VELAGUARD_IMU_DEVPATH);
  if (ret < 0)
    {
      printf("VelaGuard IMU: %s open failed: %d\n",
             CONFIG_CONTEST2026_148_VELAGUARD_IMU_DEVPATH, ret);
      return ret;
    }

  printf("VelaGuard IMU: %s addr=0x%02x whoami=0x%02x OK\n",
         CONFIG_CONTEST2026_148_VELAGUARD_IMU_DEVPATH,
         imu.addr, imu.whoami);

  vg_imu_close(&imu);
  return 0;
}
#endif /* CONFIG_CONTEST2026_148_VELAGUARD_IMU_LEGACY_I2C */
