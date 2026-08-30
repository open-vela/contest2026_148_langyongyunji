/****************************************************************************
 * contest2026_148_langyongyunji/app/hello_app/algorithm/velaguard_fall.c
 *
 * Rule-based fall detector for VelaGuard.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "velaguard_fall.h"

#include <stdio.h>
#include <string.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VG_FALL_STATE_IDLE         0
#define VG_FALL_STATE_FREEFALL     1
#define VG_FALL_STATE_IMPACT       2

#define VG_FALL_FREEFALL_MG        650
#define VG_FALL_FREEFALL_NEED_MS   40
#define VG_FALL_IMPACT_WINDOW_MS   800
#define VG_FALL_CONFIRM_WINDOW_MS  8000
#define VG_FALL_HARD_IMPACT_MG     1800
#define VG_FALL_IMPACT_MG          1400
#define VG_FALL_GYRO_DPS           700
#define VG_FALL_COMBO_MG           1200
#define VG_FALL_COMBO_GYRO_DPS     350
#define VG_FALL_POSTURE_DEG        30
#define VG_FALL_BASE_MIN_MG        750
#define VG_FALL_BASE_MAX_MG        1250
#define VG_FALL_STILL_MIN_MG       850
#define VG_FALL_STILL_MAX_MG       1150
#define VG_FALL_STILL_GYRO_DPS     110
#define VG_FALL_STILL_NEED_MS      5000
#define VG_FALL_DEBUG_LOG_MS       500

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint64_t vg_u64_abs(int64_t value)
{
  return value < 0 ? (uint64_t)-value : (uint64_t)value;
}

static uint32_t vg_isqrt64(uint64_t value)
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

int vg_fall_accel_mag_mg(const struct vg_fall_sample_s *sample)
{
  uint64_t x = vg_u64_abs(sample->ax_mg);
  uint64_t y = vg_u64_abs(sample->ay_mg);
  uint64_t z = vg_u64_abs(sample->az_mg);

  return (int)vg_isqrt64(x * x + y * y + z * z);
}

int vg_fall_gyro_sum_dps(const struct vg_fall_sample_s *sample)
{
  uint64_t x = vg_u64_abs(sample->gx_dps);
  uint64_t y = vg_u64_abs(sample->gy_dps);
  uint64_t z = vg_u64_abs(sample->gz_dps);

  return (int)vg_isqrt64(x * x + y * y + z * z);
}

static int vg_cos1000_to_deg(int cos1000)
{
  if (cos1000 >= 966)
    {
      return 15;
    }

  if (cos1000 >= 866)
    {
      return 30;
    }

  if (cos1000 >= 707)
    {
      return 45;
    }

  if (cos1000 >= 500)
    {
      return 60;
    }

  if (cos1000 >= 259)
    {
      return 75;
    }

  if (cos1000 >= 0)
    {
      return 90;
    }

  if (cos1000 >= -259)
    {
      return 105;
    }

  if (cos1000 >= -500)
    {
      return 120;
    }

  if (cos1000 >= -707)
    {
      return 135;
    }

  if (cos1000 >= -866)
    {
      return 150;
    }

  return 165;
}

static int vg_posture_delta_deg(const struct vg_fall_detector_s *detector,
                                const struct vg_fall_sample_s *sample)
{
  int64_t dot;
  uint64_t base_mag;
  uint64_t now_mag;
  int64_t cos1000;

  if (!detector->baseline_valid)
    {
      return 0;
    }

  dot = (int64_t)detector->base_x_mg * sample->ax_mg +
        (int64_t)detector->base_y_mg * sample->ay_mg +
        (int64_t)detector->base_z_mg * sample->az_mg;

  base_mag = vg_isqrt64((uint64_t)vg_u64_abs(detector->base_x_mg) *
                        vg_u64_abs(detector->base_x_mg) +
                        (uint64_t)vg_u64_abs(detector->base_y_mg) *
                        vg_u64_abs(detector->base_y_mg) +
                        (uint64_t)vg_u64_abs(detector->base_z_mg) *
                        vg_u64_abs(detector->base_z_mg));

  now_mag = vg_fall_accel_mag_mg(sample);
  if (base_mag == 0 || now_mag == 0)
    {
      return 0;
    }

  cos1000 = dot * 1000 / (int64_t)(base_mag * now_mag);
  if (cos1000 > 1000)
    {
      cos1000 = 1000;
    }
  else if (cos1000 < -1000)
    {
      cos1000 = -1000;
    }

  return vg_cos1000_to_deg((int)cos1000);
}

static void vg_update_baseline(struct vg_fall_detector_s *detector,
                               const struct vg_fall_sample_s *sample,
                               int mag_mg)
{
  if (mag_mg < VG_FALL_BASE_MIN_MG || mag_mg > VG_FALL_BASE_MAX_MG)
    {
      return;
    }

  if (vg_fall_gyro_sum_dps(sample) > VG_FALL_STILL_GYRO_DPS)
    {
      return;
    }

  if (!detector->baseline_valid)
    {
      detector->base_x_mg = sample->ax_mg;
      detector->base_y_mg = sample->ay_mg;
      detector->base_z_mg = sample->az_mg;
      detector->baseline_valid = true;
      return;
    }

  detector->base_x_mg = (detector->base_x_mg * 7 + sample->ax_mg) / 8;
  detector->base_y_mg = (detector->base_y_mg * 7 + sample->ay_mg) / 8;
  detector->base_z_mg = (detector->base_z_mg * 7 + sample->az_mg) / 8;
}

static void vg_fill_result(struct vg_fall_detector_s *detector,
                           struct vg_fall_result_s *result)
{
  memset(result, 0, sizeof(*result));
  result->detected = true;
  result->risk = 3;
  result->peak_mg = detector->peak_mg;
  result->peak_gyro_dps = detector->peak_gyro_dps;
  result->posture_delta_deg = detector->posture_delta_deg;
  result->still_ms = detector->still_ms;
  result->confidence = 58;

  if (result->peak_mg >= 2200)
    {
      result->confidence += 12;
    }
  else if (result->peak_mg >= VG_FALL_IMPACT_MG)
    {
      result->confidence += 8;
    }

  if (result->peak_gyro_dps >= VG_FALL_GYRO_DPS)
    {
      result->confidence += 10;
    }
  else if (result->peak_gyro_dps >= 450)
    {
      result->confidence += 7;
    }

  if (result->posture_delta_deg >= 75)
    {
      result->confidence += 12;
    }
  else if (result->posture_delta_deg >= 45)
    {
      result->confidence += 9;
    }
  else if (result->posture_delta_deg >= VG_FALL_POSTURE_DEG)
    {
      result->confidence += 6;
    }

  if (result->still_ms >= 1800)
    {
      result->confidence += 10;
    }
  else if (result->still_ms >= VG_FALL_STILL_NEED_MS)
    {
      result->confidence += 7;
    }

  if (detector->freefall_ms >= VG_FALL_FREEFALL_NEED_MS)
    {
      result->confidence += 4;
    }

  if (result->confidence > 96)
    {
      result->confidence = 96;
    }

  if (result->confidence >= 85)
    {
      result->risk = 4;
    }

  snprintf(result->reason, sizeof(result->reason),
           "peak=%dmg gyro=%ddps posture=%ddeg still=%dms",
           result->peak_mg, result->peak_gyro_dps,
           result->posture_delta_deg, result->still_ms);
}

static void vg_fall_log_wait(struct vg_fall_detector_s *detector,
                             const struct vg_fall_sample_s *sample,
                             int mag_mg, int gyro_dps,
                             const char *stage, const char *detail)
{
  if (detector->debug_log_ms != 0 &&
      sample->timestamp_ms - detector->debug_log_ms < VG_FALL_DEBUG_LOG_MS)
    {
      return;
    }

  detector->debug_log_ms = sample->timestamp_ms;
  printf("VelaGuard fall: %s %s mag=%d gyro=%d peak=%d/%d posture=%d still=%d state=%d\n",
         stage, detail, mag_mg, gyro_dps,
         detector->peak_mg, detector->peak_gyro_dps,
         detector->posture_delta_deg, detector->still_ms,
         detector->state);
}

static int vg_fall_sample_dt_ms(struct vg_fall_detector_s *detector,
                                const struct vg_fall_sample_s *sample)
{
  uint32_t dt;

  if (detector->last_sample_ms == 0 ||
      sample->timestamp_ms <= detector->last_sample_ms)
    {
      detector->last_sample_ms = sample->timestamp_ms;
      return 20;
    }

  dt = sample->timestamp_ms - detector->last_sample_ms;
  detector->last_sample_ms = sample->timestamp_ms;

  if (dt < 20)
    {
      return 20;
    }

  if (dt > 250)
    {
      return 250;
    }

  return (int)dt;
}

static bool vg_fall_is_direct_impact(int mag_mg, int gyro_dps)
{
  if (mag_mg >= VG_FALL_HARD_IMPACT_MG &&
      gyro_dps >= VG_FALL_COMBO_GYRO_DPS)
    {
      return true;
    }

  if (mag_mg >= VG_FALL_IMPACT_MG && gyro_dps >= VG_FALL_GYRO_DPS)
    {
      return true;
    }

  if (mag_mg >= VG_FALL_COMBO_MG && gyro_dps >= VG_FALL_COMBO_GYRO_DPS)
    {
      return true;
    }

  return false;
}

static bool vg_fall_is_freefall_impact(int mag_mg, int gyro_dps)
{
  if (mag_mg >= VG_FALL_IMPACT_MG)
    {
      return true;
    }

  if (gyro_dps >= VG_FALL_GYRO_DPS)
    {
      return true;
    }

  if (mag_mg >= VG_FALL_COMBO_MG && gyro_dps >= VG_FALL_COMBO_GYRO_DPS)
    {
      return true;
    }

  return false;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void vg_fall_init(struct vg_fall_detector_s *detector)
{
  memset(detector, 0, sizeof(*detector));
  detector->state = VG_FALL_STATE_IDLE;
}

bool vg_fall_process(struct vg_fall_detector_s *detector,
                     const struct vg_fall_sample_s *sample,
                     struct vg_fall_result_s *result)
{
  int mag_mg = vg_fall_accel_mag_mg(sample);
  int gyro_dps = vg_fall_gyro_sum_dps(sample);
  int delta_deg;
  int dt_ms;

  if (result != NULL)
    {
      memset(result, 0, sizeof(*result));
    }

  dt_ms = vg_fall_sample_dt_ms(detector, sample);
  detector->last_mg = mag_mg;
  detector->last_gyro_dps = gyro_dps;

  if (detector->state == VG_FALL_STATE_IDLE)
    {
      vg_update_baseline(detector, sample, mag_mg);

      if (mag_mg < VG_FALL_FREEFALL_MG)
        {
          if (detector->freefall_ms == 0)
            {
              detector->freefall_start_ms = sample->timestamp_ms;
            }

          detector->freefall_ms += dt_ms;
          if (detector->freefall_ms >= VG_FALL_FREEFALL_NEED_MS)
            {
              detector->state = VG_FALL_STATE_FREEFALL;
              detector->impact_ms = 0;
              detector->peak_mg = mag_mg;
              detector->peak_gyro_dps = gyro_dps;
              detector->trigger_mg = mag_mg;
              detector->trigger_gyro_dps = gyro_dps;
              detector->posture_delta_deg = 0;
              detector->still_ms = 0;
              detector->debug_log_ms = sample->timestamp_ms;
              detector->posture_changed = false;
              printf("VelaGuard fall: enter freefall mag=%d gyro=%d freefall=%lums\n",
                     mag_mg, gyro_dps,
                     (unsigned long)detector->freefall_ms);
            }
        }
      else
        {
          detector->freefall_ms = 0;
          detector->freefall_start_ms = 0;
        }

      if (vg_fall_is_direct_impact(mag_mg, gyro_dps))
        {
          detector->state = VG_FALL_STATE_IMPACT;
          detector->impact_ms = sample->timestamp_ms;
          detector->peak_mg = mag_mg;
          detector->peak_gyro_dps = gyro_dps;
          detector->trigger_mg = mag_mg;
          detector->trigger_gyro_dps = gyro_dps;
          detector->posture_delta_deg = 0;
          detector->still_ms = 0;
          detector->debug_log_ms = sample->timestamp_ms;
          detector->posture_changed = false;
          printf("VelaGuard fall: enter impact from idle mag=%d gyro=%d\n",
                 mag_mg, gyro_dps);
        }

      return false;
    }

  if (detector->state == VG_FALL_STATE_FREEFALL)
    {
      if (sample->timestamp_ms - detector->freefall_start_ms >
          VG_FALL_IMPACT_WINDOW_MS)
        {
          printf("VelaGuard fall: freefall timeout after %lums\n",
                 (unsigned long)(sample->timestamp_ms -
                                 detector->freefall_start_ms));
          vg_fall_init(detector);
          return false;
        }

      if (mag_mg < VG_FALL_FREEFALL_MG)
        {
          detector->freefall_ms += dt_ms;
          vg_fall_log_wait(detector, sample, mag_mg, gyro_dps,
                           "freefall", "waiting impact");
          return false;
        }

      if (vg_fall_is_freefall_impact(mag_mg, gyro_dps))
        {
          detector->state = VG_FALL_STATE_IMPACT;
          detector->impact_ms = sample->timestamp_ms;
          detector->peak_mg = mag_mg;
          detector->peak_gyro_dps = gyro_dps;
          detector->trigger_mg = mag_mg;
          detector->trigger_gyro_dps = gyro_dps;
          detector->posture_delta_deg = 0;
          detector->still_ms = 0;
          detector->debug_log_ms = sample->timestamp_ms;
          detector->posture_changed = false;
          printf("VelaGuard fall: enter impact after freefall mag=%d gyro=%d freefall=%lums\n",
                 mag_mg, gyro_dps,
                 (unsigned long)detector->freefall_ms);
        }
      else
        {
          vg_fall_log_wait(detector, sample, mag_mg, gyro_dps,
                           "freefall", "impact not confirmed");
        }

      return false;
    }

  if (sample->timestamp_ms - detector->impact_ms >
      VG_FALL_CONFIRM_WINDOW_MS)
    {
      printf("VelaGuard fall: impact timeout after %lums\n",
             (unsigned long)(sample->timestamp_ms - detector->impact_ms));
      vg_fall_init(detector);
      return false;
    }

  if (mag_mg > detector->peak_mg)
    {
      detector->peak_mg = mag_mg;
    }

  if (gyro_dps > detector->peak_gyro_dps)
    {
      detector->peak_gyro_dps = gyro_dps;
    }

  delta_deg = vg_posture_delta_deg(detector, sample);
  if (delta_deg > detector->posture_delta_deg)
    {
      detector->posture_delta_deg = delta_deg;
    }

  if (detector->posture_delta_deg >= VG_FALL_POSTURE_DEG)
    {
      detector->posture_changed = true;
    }

  if (detector->posture_changed &&
      mag_mg >= VG_FALL_STILL_MIN_MG &&
      mag_mg <= VG_FALL_STILL_MAX_MG &&
      gyro_dps <= VG_FALL_STILL_GYRO_DPS)
    {
      detector->still_ms += dt_ms;
    }
  else
    {
      detector->still_ms = 0;
    }

  vg_fall_log_wait(detector, sample, mag_mg, gyro_dps,
                   "impact", detector->posture_changed ?
                   "waiting stillness" : "waiting posture");

  if (detector->posture_changed &&
      detector->still_ms >= VG_FALL_STILL_NEED_MS)
    {
      if (result != NULL)
        {
          vg_fill_result(detector, result);
        }

      printf("VelaGuard fall: confirm detected peak=%d gyro=%d posture=%d still=%d\n",
             detector->peak_mg, detector->peak_gyro_dps,
             detector->posture_delta_deg, detector->still_ms);
      vg_fall_init(detector);
      return true;
    }

  return false;
}
