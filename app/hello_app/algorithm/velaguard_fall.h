/****************************************************************************
 * contest2026_148_langyongyunji/app/hello_app/algorithm/velaguard_fall.h
 *
 * Rule-based fall detector for VelaGuard.
 *
 ****************************************************************************/

#ifndef __CONTEST2026_148_LANGYONGYUNJI_APP_HELLO_APP_VELAGUARD_FALL_H
#define __CONTEST2026_148_LANGYONGYUNJI_APP_HELLO_APP_VELAGUARD_FALL_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct vg_fall_sample_s
{
  uint32_t timestamp_ms;
  int32_t ax_mg;
  int32_t ay_mg;
  int32_t az_mg;
  int32_t gx_dps;
  int32_t gy_dps;
  int32_t gz_dps;
};

struct vg_fall_result_s
{
  bool detected;
  int risk;
  int confidence;
  int peak_mg;
  int peak_gyro_dps;
  int posture_delta_deg;
  int still_ms;
  char reason[96];
};

struct vg_fall_detector_s
{
  int state;
  bool baseline_valid;
  int32_t base_x_mg;
  int32_t base_y_mg;
  int32_t base_z_mg;
  uint32_t last_sample_ms;
  uint32_t freefall_start_ms;
  uint32_t freefall_ms;
  uint32_t impact_ms;
  int last_mg;
  int last_gyro_dps;
  int peak_mg;
  int peak_gyro_dps;
  int trigger_mg;
  int trigger_gyro_dps;
  int posture_delta_deg;
  int still_ms;
  bool posture_changed;
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

void vg_fall_init(struct vg_fall_detector_s *detector);
int vg_fall_accel_mag_mg(const struct vg_fall_sample_s *sample);
int vg_fall_gyro_sum_dps(const struct vg_fall_sample_s *sample);
bool vg_fall_process(struct vg_fall_detector_s *detector,
                     const struct vg_fall_sample_s *sample,
                     struct vg_fall_result_s *result);
bool vg_fall_run_demo(struct vg_fall_result_s *result);

#endif
