/****************************************************************************
 * contest2026_148_langyongyunji/app/hello_app/sensors/velaguard_imu.h
 *
 * Minimal LSM6DS3TR-C reader for Huangshan Pi.
 *
 ****************************************************************************/

#ifndef __CONTEST2026_148_LANGYONGYUNJI_APP_HELLO_APP_VELAGUARD_IMU_H
#define __CONTEST2026_148_LANGYONGYUNJI_APP_HELLO_APP_VELAGUARD_IMU_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct vg_imu_s
{
  int fd;
  uint8_t addr;
  uint8_t whoami;
};

struct vg_imu_sample_s
{
  uint32_t timestamp_ms;
  int32_t ax_mg;
  int32_t ay_mg;
  int32_t az_mg;
  int32_t gx_dps;
  int32_t gy_dps;
  int32_t gz_dps;
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

int vg_imu_open(struct vg_imu_s *imu, const char *devpath);
int vg_imu_open_guarded(struct vg_imu_s *imu, const char *devpath);
void vg_imu_close(struct vg_imu_s *imu);
int vg_imu_read(struct vg_imu_s *imu, struct vg_imu_sample_s *sample);
int vg_imu_read_guarded(struct vg_imu_s *imu,
                        struct vg_imu_sample_s *sample);
int vg_imu_scan(void);
int vg_imu_selftest(const char *devpath, int nsamples);
int vg_imu_fall_watch(const char *devpath, int seconds);

#endif
