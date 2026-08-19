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

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
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
#define LSM6DS3_CTRL1_XL      0x10
#define LSM6DS3_CTRL2_G       0x11
#define LSM6DS3_CTRL3_C       0x12
#define LSM6DS3_OUTX_L_G      0x22

#define VG_IMU_FALL_PERIOD_MS 50
#define VG_IMU_PIN_SETTLE_US  500

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

extern void BSP_GPIO_Set(int pin, int val, int is_porta);
extern void BSP_PIN_Touch(void);

__attribute__((weak))
int sf32lb52_i2c1_mux_lock(void)
{
  return 0;
}

__attribute__((weak))
int sf32lb52_i2c1_mux_unlock(void)
{
  return 0;
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
                           uint8_t *buffer, uint8_t length)
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

  /* BDU + auto-increment register address. */

  ret = vg_imu_write_reg(imu->fd, imu->addr, LSM6DS3_CTRL3_C, 0x44);
  if (ret < 0)
    {
      return ret;
    }

  /* Accelerometer: 104 Hz, +-8 g. */

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

  usleep(30000);
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

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

int vg_imu_read(struct vg_imu_s *imu, struct vg_imu_sample_s *sample)
{
  uint8_t data[12];
  int16_t gx;
  int16_t gy;
  int16_t gz;
  int16_t ax;
  int16_t ay;
  int16_t az;
  int ret;

  ret = vg_imu_read_reg(imu->fd, imu->addr, LSM6DS3_OUTX_L_G,
                        data, sizeof(data));
  if (ret < 0)
    {
      return ret;
    }

  gx = vg_i16_le(&data[0]);
  gy = vg_i16_le(&data[2]);
  gz = vg_i16_le(&data[4]);
  ax = vg_i16_le(&data[6]);
  ay = vg_i16_le(&data[8]);
  az = vg_i16_le(&data[10]);

  sample->timestamp_ms = vg_imu_uptime_ms();
  sample->ax_mg = ((int32_t)ax * 244 + (ax >= 0 ? 500 : -500)) / 1000;
  sample->ay_mg = ((int32_t)ay * 244 + (ay >= 0 ? 500 : -500)) / 1000;
  sample->az_mg = ((int32_t)az * 244 + (az >= 0 ? 500 : -500)) / 1000;
  sample->gx_dps = ((int32_t)gx * 70 + (gx >= 0 ? 500 : -500)) / 1000;
  sample->gy_dps = ((int32_t)gy * 70 + (gy >= 0 ? 500 : -500)) / 1000;
  sample->gz_dps = ((int32_t)gz * 70 + (gz >= 0 ? 500 : -500)) / 1000;

  return 0;
}

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

  printf("VelaGuard IMU: switch I2C1 pins to IMU SCL=PA40 SDA=PA39 PWR=PA30\n");
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
  printf("VelaGuard Fall: actions: keep still 2s, simulate fall on soft pad, then keep still 2s\n");
  printf("VelaGuard IMU: switch I2C1 pins to IMU SCL=PA40 SDA=PA39 PWR=PA30\n");
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
