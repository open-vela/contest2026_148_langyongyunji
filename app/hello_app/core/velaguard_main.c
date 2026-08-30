/****************************************************************************
 * contest2026_148_langyongyunji/app/hello_app/core/velaguard_main.c
 *
 * VelaGuard - end-side AI safety guardian prototype for openvela.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/sched.h>

#include <sys/boardctl.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>

#ifdef CONFIG_INPUT_BUTTONS
#  include <nuttx/input/buttons.h>
#endif
#ifdef CONFIG_INPUT_TOUCHSCREEN
#  include <nuttx/input/touchscreen.h>
#endif

#include <lvgl/lvgl.h>

extern const lv_image_dsc_t velaguard_img_bg_alarm;
extern const lv_image_dsc_t velaguard_img_ble_icon;
extern const lv_image_dsc_t velaguard_img_fall_icon;
extern const lv_image_dsc_t velaguard_img_alarm_clock_0;
extern const lv_image_dsc_t velaguard_img_count_0;
extern const lv_image_dsc_t velaguard_img_count_1;
extern const lv_image_dsc_t velaguard_img_count_2;
extern const lv_image_dsc_t velaguard_img_count_3;
extern const lv_image_dsc_t velaguard_img_count_4;
extern const lv_image_dsc_t velaguard_img_count_5;
extern const lv_image_dsc_t velaguard_img_count_6;
extern const lv_image_dsc_t velaguard_img_count_7;
extern const lv_image_dsc_t velaguard_img_count_8;
extern const lv_image_dsc_t velaguard_img_count_9;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_bg;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_battery_5;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_white_0;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_white_1;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_white_2;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_white_3;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_white_4;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_white_5;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_white_6;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_white_7;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_white_8;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_white_9;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_blue_0;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_blue_1;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_blue_2;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_blue_3;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_blue_4;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_blue_5;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_blue_6;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_blue_7;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_blue_8;
extern const lv_image_dsc_t velaguard_img_icon_rainbow_rain_blue_9;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_illustration;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_colon;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_battery_5;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_hour_0;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_hour_1;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_hour_2;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_hour_3;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_hour_4;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_hour_5;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_hour_6;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_hour_7;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_hour_8;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_hour_9;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_date_0;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_date_1;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_date_2;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_date_3;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_date_4;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_date_5;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_date_6;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_date_7;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_date_8;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_date_9;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_date_slash;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_week_0;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_week_1;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_week_2;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_week_3;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_week_4;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_week_5;
extern const lv_image_dsc_t velaguard_img_icon_touch_future_week_6;
extern const lv_image_dsc_t velaguard_img_thumb_rainbow_rain;
extern const lv_image_dsc_t velaguard_img_thumb_touch_future;

#include "velaguard_fall.h"
#include "velaguard_imu.h"
#include "velaguard_ble.h"
#include "velaguard_audio.h"
#include "velaguard_audio_feedback.h"

LV_FONT_DECLARE(velaguard_font_30);

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_CONTEST2026_148_VELAGUARD_COUNTDOWN
#  define CONFIG_CONTEST2026_148_VELAGUARD_COUNTDOWN 10
#endif

#ifndef CONFIG_CONTEST2026_148_VELAGUARD_HISTORY_SIZE
#  define CONFIG_CONTEST2026_148_VELAGUARD_HISTORY_SIZE 5
#endif

#ifndef CONFIG_CONTEST2026_148_VELAGUARD_LOOP_SLEEP_MAX_MS
#  define CONFIG_CONTEST2026_148_VELAGUARD_LOOP_SLEEP_MAX_MS 8
#endif

#ifndef CONFIG_CONTEST2026_148_VELAGUARD_INPUT_POLL_MS
#  define CONFIG_CONTEST2026_148_VELAGUARD_INPUT_POLL_MS 16
#endif

#ifndef CONFIG_CONTEST2026_148_VELAGUARD_INPUT_DEVPATH
#  define CONFIG_CONTEST2026_148_VELAGUARD_INPUT_DEVPATH "/dev/input0"
#endif

#ifndef CONFIG_CONTEST2026_148_VELAGUARD_BUTTON_DEVPATH
#  define CONFIG_CONTEST2026_148_VELAGUARD_BUTTON_DEVPATH "/dev/buttons"
#endif

#ifndef CONFIG_CONTEST2026_148_VELAGUARD_IMU_DEVPATH
#  define CONFIG_CONTEST2026_148_VELAGUARD_IMU_DEVPATH "/dev/lsm6dsl0"
#endif

#if CONFIG_CONTEST2026_148_VELAGUARD_HISTORY_SIZE < 1
#  define VG_HISTORY_SIZE 1
#else
#  define VG_HISTORY_SIZE CONFIG_CONTEST2026_148_VELAGUARD_HISTORY_SIZE
#endif

#undef NEED_BOARDINIT

#if defined(CONFIG_BOARDCTL) && !defined(CONFIG_NSH_ARCHINIT)
#  define NEED_BOARDINIT 1
#endif

#define VG_COLOR_BG          0x121820
#define VG_COLOR_CARD        0x1d2833
#define VG_COLOR_CARD_ALT    0x243241
#define VG_COLOR_TEXT        0xf5f7fb
#define VG_COLOR_MUTED       0xaab4c0
#define VG_COLOR_OK          0x2fbf71
#define VG_COLOR_WARN        0xf0a33a
#define VG_COLOR_ALERT       0xe84d5b
#define VG_COLOR_INFO        0x3d8bfd
#define VG_TICK_PERIOD_MS    10
#define VG_IMU_UI_PERIOD_MS  100
#define VG_SOS_BUTTON_BIT    ((btn_buttonset_t)1 << 0) /* PA43 / KEY2 */
#define VG_DEVICE_WAIT_STEP_MS 100
#define VG_DEVICE_WAIT_MS      5000
#define VG_VOICE_REARM_MS       3000
#define VG_LIGHTWEIGHT_UI       0
#define VG_LOCAL_TIME_OFFSET_SECONDS (8 * 60 * 60)
#define VG_BASE_W               240
#define VG_BASE_H               280
#define VG_SCREEN_W             390
#define VG_SCREEN_H             450
#define VG_X(v)                 ((int32_t)(((v) * VG_SCREEN_W + VG_BASE_W / 2) / VG_BASE_W))
#define VG_Y(v)                 ((int32_t)(((v) * VG_SCREEN_H + VG_BASE_H / 2) / VG_BASE_H))
#define VG_SOS_LONG_MS          1200
#define VG_BUTTON_RELEASE_DEBOUNCE_MS 100
#define VG_HOME_EDIT_LONG_MS    1200
#define VG_HOME_GESTURE_SLOP_PX VG_X(12)
#define VG_HOLD_CONFIRM_MS      3000
#define VG_ALARM_FRAME_COUNT    1

/****************************************************************************
 * Private Types
 ****************************************************************************/

enum vg_state_e
{
  VG_STATE_GUARDING = 0,
  VG_STATE_PREALERT,
  VG_STATE_ALERTING,
};

enum vg_event_type_e
{
  VG_EVENT_MANUAL_SOS = 0,
  VG_EVENT_FALL,
  VG_EVENT_SOUND,
  VG_EVENT_VOICE,
};

enum vg_action_e
{
  VG_ACTION_SOS = 1,
  VG_ACTION_DEMO_FALL,
  VG_ACTION_DEMO_SOUND,
  VG_ACTION_DEMO_VOICE,
  VG_ACTION_CANCEL,
  VG_ACTION_CONFIRM,
  VG_ACTION_RESOLVE,
  VG_ACTION_HISTORY,
  VG_ACTION_SETTINGS,
  VG_ACTION_BLUETOOTH,
  VG_ACTION_BLE_TOGGLE,
  VG_ACTION_FALL_CANCEL_HOLD,
  VG_ACTION_FALL_CONFIRM_HOLD,
  VG_ACTION_DIAL_RAINBOW,
  VG_ACTION_DIAL_SIMPLE,
  VG_ACTION_MODE,
  VG_ACTION_BACK,
};

enum vg_page_e
{
  VG_PAGE_HOME = 0,
  VG_PAGE_BLUETOOTH,
  VG_PAGE_WATCHFACE_PICKER,
};

/* A page rebuild deletes the current screen children.  It must therefore not
 * run from an LVGL event or timer callback which can still reference them. */
enum vg_render_e
{
  VG_RENDER_NONE = 0,
  VG_RENDER_HOME,
  VG_RENDER_PREALERT,
  VG_RENDER_ALERT,
  VG_RENDER_HISTORY,
  VG_RENDER_SETTINGS,
  VG_RENDER_BLUETOOTH,
  VG_RENDER_WATCHFACE,
  VG_RENDER_CURRENT,
};

enum vg_mode_e
{
  VG_MODE_ELDER = 0,
  VG_MODE_OUTDOOR,
  VG_MODE_CAMPUS,
  VG_MODE_WORKSITE,
};

struct vg_event_s
{
  uint32_t id;
  enum vg_event_type_e type;
  uint64_t timestamp_ms;
  int risk;
  int confidence;
  char phase[16];
  char summary[176];
};

struct vg_app_s
{
  enum vg_state_e state;
  enum vg_mode_e mode;
  struct vg_event_s active;
  struct vg_event_s history[VG_HISTORY_SIZE];
  struct vg_fall_result_s last_fall;
  struct vg_fall_detector_s fall_detector;
  struct vg_imu_s imu;
  int history_count;
  int history_head;
  int countdown;
  int countdown_total;
  int tick_accum_ms;
  int countdown_last_value;
  int imu_mag_mg;
  int imu_gyro_dps;
  int imu_last_error;
  struct vg_audio_level_s audio_level;
  enum vg_audio_keyword_e audio_keyword;
  uint32_t audio_sequence;
  uint32_t audio_report_tick;
  uint64_t voice_last_trigger_ms;
  uint64_t button_down_ms;
  uint64_t button_arm_release_ms;
  uint64_t button_release_candidate_ms;
  uint64_t countdown_start_ms;
  uint64_t ble_toggle_last_ms;
  uint64_t bluetooth_last_refresh_ms;
  uint64_t home_press_start_ms;
  lv_point_t home_press_point;
  uint64_t hold_cancel_start_ms;
  uint64_t hold_confirm_start_ms;
  enum vg_action_e hold_action;
  int hold_last_value;
  bool audio_ready;
  bool audio_start_attempted;
  bool audio_resume_after_feedback;
  uint32_t next_id;
  bool has_fall_result;
  bool imu_ready;
  bool button_armed;
  bool button_long_handled;
  bool sos_prompt_visible;
  bool hold_cancel_active;
  bool hold_confirm_active;
  bool navigating;
  bool gesture_consumed;
  bool home_press_moved;
  bool home_edit_handled;
  enum vg_render_e pending_render;
  enum vg_page_e current_page;
  enum vg_page_e target_page;
  uint8_t watchface;
  int home_last_minute;
  int home_last_mday;
  int home_time_digit[4];
  int home_date_digit[4];
  int home_week_digit;
  int alarm_frame;
  lv_obj_t *countdown_label;
  lv_obj_t *countdown_arc;
  lv_obj_t *countdown_single_img;
  lv_obj_t *countdown_tens_img;
  lv_obj_t *countdown_ones_img;
  lv_obj_t *hold_overlay;
  lv_obj_t *hold_arc;
  lv_obj_t *hold_digit_img;
  lv_obj_t *alarm_img;
  lv_obj_t *home_time_img[4];
  lv_obj_t *home_date_img[4];
  lv_obj_t *home_week_img;
  lv_obj_t *home_date_label;
  lv_obj_t *home_week_label;
  lv_obj_t *home_root;
  lv_obj_t *bluetooth_root;
  lv_obj_t *bluetooth_addr_label;
  lv_obj_t *detail_label;
  lv_obj_t *imu_status_label;
  lv_obj_t *imu_value_label;
  lv_obj_t *imu_detail_label;
  lv_timer_t *tick_timer;
  lv_timer_t *imu_timer;
  lv_timer_t *render_timer;
#ifdef CONFIG_INPUT_BUTTONS
  int button_fd;
  btn_buttonset_t last_buttons;
#endif
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void vg_render_home(void);
static void vg_render_prealert(void);
static void vg_render_alert(void);
static void vg_render_history(void);
static void vg_render_settings(void);
static void vg_render_bluetooth(void);
static void vg_create_bluetooth_page(lv_obj_t *scr);
static void __attribute__((unused)) vg_render_watchface_picker(void);
static void vg_render_current(void);
static void vg_navigation_gesture_cb(lv_event_t *event);
static const char *vg_page_name(enum vg_page_e page);
static void vg_nav_request(enum vg_page_e target, lv_dir_t dir,
                           const char *source);
static void vg_schedule_render(enum vg_render_e render);
static void vg_process_pending_render(void);
static void vg_render_timer_cb(lv_timer_t *timer);
static uint64_t vg_uptime_ms(void);
static void vg_set_font(lv_obj_t *obj);
static void vg_trigger_fall_result(const struct vg_fall_result_s *result);
static void vg_start_prealert(enum vg_event_type_e type, int countdown,
                              int risk, int confidence);
static void vg_update_imu_labels(void);
static void vg_update_watchface_time(void);
static void vg_update_countdown_visuals(void);
static void vg_update_manual_sos_progress(uint64_t now);
static void vg_update_bluetooth_page(bool force);
static void vg_update_alarm_hold(void);
static void vg_render_fall_hold_progress(enum vg_action_e action);
static void vg_update_hold_progress_visuals(uint64_t now);
static void vg_audio_process(void);
static void vg_audio_start_once(void);
static void vg_audio_feedback_start(enum vg_feedback_type_e type);
static void vg_audio_resume_after_feedback(void);
static void vg_ble_service_poll(void);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct vg_app_s g_vg;

static const lv_image_dsc_t *g_alarm_frames[VG_ALARM_FRAME_COUNT] =
{
  &velaguard_img_alarm_clock_0,
};

static const lv_image_dsc_t *g_count_digits[10] =
{
  &velaguard_img_count_0,
  &velaguard_img_count_1,
  &velaguard_img_count_2,
  &velaguard_img_count_3,
  &velaguard_img_count_4,
  &velaguard_img_count_5,
  &velaguard_img_count_6,
  &velaguard_img_count_7,
  &velaguard_img_count_8,
  &velaguard_img_count_9,
};

static const char *g_week_names[7] =
{
  "周日",
  "周一",
  "周二",
  "周三",
  "周四",
  "周五",
  "周六",
};

static const lv_image_dsc_t *g_rainbow_white_digits[10] =
{
  &velaguard_img_icon_rainbow_rain_white_0,
  &velaguard_img_icon_rainbow_rain_white_1,
  &velaguard_img_icon_rainbow_rain_white_2,
  &velaguard_img_icon_rainbow_rain_white_3,
  &velaguard_img_icon_rainbow_rain_white_4,
  &velaguard_img_icon_rainbow_rain_white_5,
  &velaguard_img_icon_rainbow_rain_white_6,
  &velaguard_img_icon_rainbow_rain_white_7,
  &velaguard_img_icon_rainbow_rain_white_8,
  &velaguard_img_icon_rainbow_rain_white_9,
};

static const lv_image_dsc_t *g_rainbow_blue_digits[10] =
{
  &velaguard_img_icon_rainbow_rain_blue_0,
  &velaguard_img_icon_rainbow_rain_blue_1,
  &velaguard_img_icon_rainbow_rain_blue_2,
  &velaguard_img_icon_rainbow_rain_blue_3,
  &velaguard_img_icon_rainbow_rain_blue_4,
  &velaguard_img_icon_rainbow_rain_blue_5,
  &velaguard_img_icon_rainbow_rain_blue_6,
  &velaguard_img_icon_rainbow_rain_blue_7,
  &velaguard_img_icon_rainbow_rain_blue_8,
  &velaguard_img_icon_rainbow_rain_blue_9,
};

static const lv_image_dsc_t *g_touch_hour_digits[10] =
{
  &velaguard_img_icon_touch_future_hour_0,
  &velaguard_img_icon_touch_future_hour_1,
  &velaguard_img_icon_touch_future_hour_2,
  &velaguard_img_icon_touch_future_hour_3,
  &velaguard_img_icon_touch_future_hour_4,
  &velaguard_img_icon_touch_future_hour_5,
  &velaguard_img_icon_touch_future_hour_6,
  &velaguard_img_icon_touch_future_hour_7,
  &velaguard_img_icon_touch_future_hour_8,
  &velaguard_img_icon_touch_future_hour_9,
};

static const lv_image_dsc_t *g_touch_date_digits[10] =
{
  &velaguard_img_icon_touch_future_date_0,
  &velaguard_img_icon_touch_future_date_1,
  &velaguard_img_icon_touch_future_date_2,
  &velaguard_img_icon_touch_future_date_3,
  &velaguard_img_icon_touch_future_date_4,
  &velaguard_img_icon_touch_future_date_5,
  &velaguard_img_icon_touch_future_date_6,
  &velaguard_img_icon_touch_future_date_7,
  &velaguard_img_icon_touch_future_date_8,
  &velaguard_img_icon_touch_future_date_9,
};

static const lv_image_dsc_t *g_touch_week_digits[7] =
{
  &velaguard_img_icon_touch_future_week_0,
  &velaguard_img_icon_touch_future_week_1,
  &velaguard_img_icon_touch_future_week_2,
  &velaguard_img_icon_touch_future_week_3,
  &velaguard_img_icon_touch_future_week_4,
  &velaguard_img_icon_touch_future_week_5,
  &velaguard_img_icon_touch_future_week_6,
};

static void vg_ble_service_poll(void)
{
  /* Framework setup is performed once during application startup.  This loop
   * only advances asynchronous GATT and advertising state. */
  vg_ble_process();
}

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint64_t vg_uptime_ms(void)
{
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
    {
      return 0;
    }

  return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static const char *vg_event_json(enum vg_event_type_e type)
{
  switch (type)
    {
      case VG_EVENT_MANUAL_SOS:
        return "manual_sos";

      case VG_EVENT_FALL:
        return "fall_suspected";

      case VG_EVENT_SOUND:
        return "sound_abnormal";

      case VG_EVENT_VOICE:
        return "voice_sos";

      default:
        return "unknown";
    }
}

static const char *vg_event_title(enum vg_event_type_e type)
{
  switch (type)
    {
      case VG_EVENT_MANUAL_SOS:
        return "手动求助";

      case VG_EVENT_FALL:
        return "疑似跌倒";

      case VG_EVENT_SOUND:
        return "异常声音";

      case VG_EVENT_VOICE:
        return "语音求助";

      default:
        return "未知事件";
    }
}

static const char *vg_mode_name(enum vg_mode_e mode)
{
  switch (mode)
    {
      case VG_MODE_ELDER:
        return "老人";

      case VG_MODE_OUTDOOR:
        return "户外";

      case VG_MODE_CAMPUS:
        return "校园";

      case VG_MODE_WORKSITE:
        return "工地";

      default:
        return "老人";
    }
}

static const char *vg_phase_title(const char *phase)
{
  if (strcmp(phase, "suspected") == 0)
    {
      return "疑似";
    }

  if (strcmp(phase, "alert") == 0)
    {
      return "告警";
    }

  if (strcmp(phase, "cancelled") == 0)
    {
      return "取消";
    }

  if (strcmp(phase, "resolved") == 0)
    {
      return "解除";
    }

  return "事件";
}

static void vg_make_summary(struct vg_event_s *event, const char *phase)
{
  const char *mode = vg_mode_name(g_vg.mode);

  switch (event->type)
    {
      case VG_EVENT_MANUAL_SOS:
        snprintf(event->summary, sizeof(event->summary),
                 "%s模式：用户按下SOS，本地告警与事件上报已启动。",
                 mode);
        break;

      case VG_EVENT_FALL:
        if (g_vg.has_fall_result)
          {
            snprintf(event->summary, sizeof(event->summary),
                     "%s模式：IMU峰值%d.%02dg，角速度%d dps，姿态变化约%d度，静止%dms，判定疑似跌倒。",
                     mode, g_vg.last_fall.peak_mg / 1000,
                     (g_vg.last_fall.peak_mg % 1000) / 10,
                     g_vg.last_fall.peak_gyro_dps,
                     g_vg.last_fall.posture_delta_deg,
                     g_vg.last_fall.still_ms);
          }
        else
          {
            snprintf(event->summary, sizeof(event->summary),
                     "%s模式：检测到冲击、姿态变化和静止窗口，判定为疑似跌倒。",
                     mode);
          }
        break;

      case VG_EVENT_SOUND:
        snprintf(event->summary, sizeof(event->summary),
                 "%s模式：麦克风能量持续偏高，匹配尖叫或撞击声规则。",
                 mode);
        break;

      case VG_EVENT_VOICE:
        snprintf(event->summary, sizeof(event->summary),
                 "%s模式：检测到唤醒词和求助指令，进入语音SOS流程。",
                 mode);
        break;

      default:
        snprintf(event->summary, sizeof(event->summary),
                 "%s模式：未知事件。", mode);
        break;
    }

  strlcpy(event->phase, phase, sizeof(event->phase));
}

static void vg_emit_json(const struct vg_event_s *event)
{
  printf("VELAGUARD_EVENT "
         "{\"app\":\"VelaGuard\",\"id\":%" PRIu32
         ",\"phase\":\"%s\",\"type\":\"%s\",\"uptime_ms\":%" PRIu64
         ",\"risk\":%d,\"confidence\":%d,\"summary\":\"%s\"}\n",
         event->id, event->phase, vg_event_json(event->type),
         event->timestamp_ms, event->risk, event->confidence,
         event->summary);
  fflush(stdout);
}

static void vg_history_push(const struct vg_event_s *event)
{
  g_vg.history[g_vg.history_head] = *event;
  g_vg.history_head = (g_vg.history_head + 1) % VG_HISTORY_SIZE;

  if (g_vg.history_count < VG_HISTORY_SIZE)
    {
      g_vg.history_count++;
    }
}

static void vg_prepare_event(enum vg_event_type_e type, int risk,
                             int confidence, const char *phase)
{
  memset(&g_vg.active, 0, sizeof(g_vg.active));
  g_vg.active.id = ++g_vg.next_id;
  g_vg.active.type = type;
  g_vg.active.timestamp_ms = vg_uptime_ms();
  g_vg.active.risk = risk;
  g_vg.active.confidence = confidence;
  vg_make_summary(&g_vg.active, phase);
}

static void vg_confirm_alert(void)
{
  uint8_t ble_type;

  if (g_vg.state == VG_STATE_ALERTING)
    {
      return;
    }

  g_vg.state = VG_STATE_ALERTING;
  vg_ble_set_fall_status(g_vg.active.type == VG_EVENT_FALL);
  g_vg.countdown = 0;
  g_vg.countdown_total = 0;
  g_vg.tick_accum_ms = 0;
  g_vg.countdown_last_value = 0;
  g_vg.countdown_start_ms = 0;
  g_vg.sos_prompt_visible = false;
  g_vg.hold_cancel_active = false;
  g_vg.hold_confirm_active = false;
  g_vg.hold_action = 0;
  g_vg.hold_last_value = -1;
  g_vg.active.timestamp_ms = vg_uptime_ms();
  vg_make_summary(&g_vg.active, "alert");
  vg_history_push(&g_vg.active);
  vg_emit_json(&g_vg.active);

  ble_type = g_vg.active.type == VG_EVENT_FALL ? VG_BLE_EVENT_FALL :
             g_vg.active.type == VG_EVENT_VOICE ? VG_BLE_EVENT_VOICE_SOS :
             VG_BLE_EVENT_MANUAL_SOS;
  {
    int ble_ret = vg_ble_request_call(ble_type, g_vg.active.risk,
                        g_vg.active.confidence, g_vg.active.id,
                        (uint32_t)g_vg.active.timestamp_ms, true);
#ifdef CONFIG_CONTEST2026_148_AUDIO_FEEDBACK
    if (ble_ret == 0 || ble_ret == -ENOTCONN)
      {
        vg_audio_feedback_start(VG_FEEDBACK_SUCCESS);
      }
    else
      {
        vg_audio_feedback_start(VG_FEEDBACK_FAILURE);
      }
#endif
  }
  vg_schedule_render(VG_RENDER_ALERT);
}

static void vg_trigger_event(enum vg_event_type_e type)
{
  int confidence = 88;

  g_vg.has_fall_result = false;

  if (type == VG_EVENT_MANUAL_SOS)
    {
      vg_start_prealert(VG_EVENT_MANUAL_SOS, 5, 4, 100);
      return;
    }

  if (type == VG_EVENT_VOICE)
    {
      vg_prepare_event(type, 4,
                       type == VG_EVENT_MANUAL_SOS ? 100 : 92, "alert");
      g_vg.state = VG_STATE_ALERTING;
      vg_history_push(&g_vg.active);
      vg_emit_json(&g_vg.active);
      {
        int ble_ret = vg_ble_request_call(type == VG_EVENT_VOICE ?
                            VG_BLE_EVENT_VOICE_SOS :
                            VG_BLE_EVENT_MANUAL_SOS, g_vg.active.risk,
                            g_vg.active.confidence, g_vg.active.id,
                            (uint32_t)g_vg.active.timestamp_ms, true);
#ifdef CONFIG_CONTEST2026_148_AUDIO_FEEDBACK
        if (ble_ret == 0 || ble_ret == -ENOTCONN)
          {
            vg_audio_feedback_start(VG_FEEDBACK_SUCCESS);
          }
        else
          {
            vg_audio_feedback_start(VG_FEEDBACK_FAILURE);
          }
#endif
      }
      vg_schedule_render(VG_RENDER_ALERT);
      return;
    }

  if (type == VG_EVENT_FALL)
    {
      vg_start_prealert(VG_EVENT_FALL, 30, 3, confidence);
      return;
    }

  if (type == VG_EVENT_SOUND)
    {
      confidence = 76;
    }

  vg_start_prealert(type, CONFIG_CONTEST2026_148_VELAGUARD_COUNTDOWN,
                    3, confidence);
}

static void vg_start_prealert(enum vg_event_type_e type, int countdown,
                              int risk, int confidence)
{
  vg_prepare_event(type, risk, confidence, "suspected");
  g_vg.state = VG_STATE_PREALERT;
  g_vg.countdown = countdown;
  g_vg.countdown_total = countdown;
  g_vg.tick_accum_ms = 0;
  g_vg.countdown_last_value = countdown;
  g_vg.countdown_start_ms = vg_uptime_ms();
  g_vg.sos_prompt_visible = false;
  g_vg.hold_cancel_active = false;
  g_vg.hold_confirm_active = false;
  g_vg.hold_action = 0;
  g_vg.hold_last_value = -1;
  g_vg.alarm_frame = 0;
  vg_emit_json(&g_vg.active);
  vg_schedule_render(VG_RENDER_PREALERT);
}

static void vg_popup_close_cb(lv_event_t *event)
{
  lv_obj_t *mbox;

  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
    {
      return;
    }

  mbox = (lv_obj_t *)lv_event_get_user_data(event);
  if (mbox != NULL)
    {
      lv_msgbox_close(mbox);
    }
}

static void __attribute__((unused))
vg_show_fall_popup(const struct vg_fall_result_s *result)
{
  lv_obj_t *mbox;
  lv_obj_t *obj;
  char line[160];

  mbox = lv_msgbox_create(NULL);
  vg_set_font(mbox);
  lv_obj_set_width(mbox, lv_display_get_horizontal_resolution(NULL) - 64);
  lv_obj_set_style_bg_color(mbox, lv_color_hex(VG_COLOR_CARD),
                            LV_PART_MAIN);
  lv_obj_set_style_text_color(mbox, lv_color_hex(VG_COLOR_TEXT),
                              LV_PART_MAIN);

  obj = lv_msgbox_add_title(mbox, "检测到跌倒");
  vg_set_font(obj);

  snprintf(line, sizeof(line),
           "IMU %dmg\nGyro %ddps\n姿态 %d度  静止 %dms\n已进入确认。",
           result->peak_mg, result->peak_gyro_dps,
           result->posture_delta_deg, result->still_ms);
  obj = lv_msgbox_add_text(mbox, line);
  vg_set_font(obj);

  obj = lv_msgbox_add_footer_button(mbox, "确认");
  vg_set_font(obj);
  lv_obj_add_event_cb(obj, vg_popup_close_cb, LV_EVENT_CLICKED, mbox);
}

static void vg_trigger_fall_result(const struct vg_fall_result_s *result)
{
  if (g_vg.state != VG_STATE_GUARDING)
    {
      return;
    }

  g_vg.last_fall = *result;
  g_vg.has_fall_result = true;

  vg_start_prealert(VG_EVENT_FALL, 30, result->risk, result->confidence);
}

static void vg_cancel_event(void)
{
  if (g_vg.state != VG_STATE_PREALERT)
    {
      vg_schedule_render(VG_RENDER_HOME);
      return;
    }

  g_vg.active.timestamp_ms = vg_uptime_ms();
  g_vg.active.risk = 0;
  vg_make_summary(&g_vg.active, "cancelled");
  vg_history_push(&g_vg.active);
  vg_emit_json(&g_vg.active);

  g_vg.state = VG_STATE_GUARDING;
  g_vg.tick_accum_ms = 0;
  g_vg.countdown = 0;
  g_vg.countdown_total = 0;
  g_vg.countdown_last_value = 0;
  g_vg.countdown_start_ms = 0;
  g_vg.has_fall_result = false;
  vg_ble_set_fall_status(false);
  g_vg.sos_prompt_visible = false;
  g_vg.hold_cancel_active = false;
  g_vg.hold_confirm_active = false;
  g_vg.hold_action = 0;
  g_vg.hold_last_value = -1;
  vg_schedule_render(VG_RENDER_HOME);
}

static void vg_resolve_event(void)
{
  if (g_vg.state == VG_STATE_ALERTING)
    {
      g_vg.active.timestamp_ms = vg_uptime_ms();
      vg_make_summary(&g_vg.active, "resolved");
      vg_emit_json(&g_vg.active);
    }

  g_vg.state = VG_STATE_GUARDING;
  g_vg.tick_accum_ms = 0;
  g_vg.countdown = 0;
  g_vg.countdown_total = 0;
  g_vg.countdown_last_value = 0;
  g_vg.countdown_start_ms = 0;
  g_vg.has_fall_result = false;
  vg_ble_set_fall_status(false);
  g_vg.sos_prompt_visible = false;
  g_vg.hold_cancel_active = false;
  g_vg.hold_confirm_active = false;
  g_vg.hold_action = 0;
  g_vg.hold_last_value = -1;
  vg_schedule_render(VG_RENDER_HOME);
}

static lv_obj_t *vg_screen_reset(void)
{
  lv_obj_t *scr = lv_screen_active();
  int i;

  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(VG_COLOR_BG), LV_PART_MAIN);
  lv_obj_set_style_text_color(scr, lv_color_hex(VG_COLOR_TEXT),
                              LV_PART_MAIN);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  g_vg.countdown_label = NULL;
  g_vg.countdown_arc = NULL;
  g_vg.countdown_single_img = NULL;
  g_vg.countdown_tens_img = NULL;
  g_vg.countdown_ones_img = NULL;
  g_vg.hold_overlay = NULL;
  g_vg.hold_arc = NULL;
  g_vg.hold_digit_img = NULL;
  g_vg.alarm_img = NULL;
  for (i = 0; i < 4; i++)
    {
      g_vg.home_time_img[i] = NULL;
      g_vg.home_date_img[i] = NULL;
      g_vg.home_time_digit[i] = -1;
      g_vg.home_date_digit[i] = -1;
    }

  g_vg.home_week_img = NULL;
  g_vg.home_week_digit = -1;
  g_vg.home_date_label = NULL;
  g_vg.home_week_label = NULL;
  g_vg.home_root = NULL;
  g_vg.bluetooth_root = NULL;
  g_vg.bluetooth_addr_label = NULL;
  g_vg.bluetooth_last_refresh_ms = 0;
  g_vg.detail_label = NULL;
  g_vg.imu_status_label = NULL;
  g_vg.imu_value_label = NULL;
  g_vg.imu_detail_label = NULL;

  return scr;
}

static void vg_schedule_render(enum vg_render_e render)
{
  /* Multiple state changes in one LVGL pass only need the final page. */
  g_vg.pending_render = render;

  /* Keep page creation and deletion in LVGL's timer context. */
  if (g_vg.render_timer != NULL)
    {
      /* lv_timer_ready() does not run a paused timer.  Rendering is paused
       * while idle, so resume it before scheduling this LVGL pass. */
      lv_timer_resume(g_vg.render_timer);
      lv_timer_ready(g_vg.render_timer);
    }
}

static void vg_process_pending_render(void)
{
  enum vg_render_e render = g_vg.pending_render;

  if (render == VG_RENDER_NONE)
    {
      return;
    }

  g_vg.pending_render = VG_RENDER_NONE;
  switch (render)
    {
      case VG_RENDER_HOME:
        vg_render_home();
        break;

      case VG_RENDER_PREALERT:
        vg_render_prealert();
        break;

      case VG_RENDER_ALERT:
        vg_render_alert();
        break;

      case VG_RENDER_HISTORY:
        vg_render_history();
        break;

      case VG_RENDER_SETTINGS:
        vg_render_settings();
        break;

      case VG_RENDER_BLUETOOTH:
        vg_render_bluetooth();
        break;

      case VG_RENDER_WATCHFACE:
        vg_render_watchface_picker();
        break;

      case VG_RENDER_CURRENT:
        vg_render_current();
        break;

      case VG_RENDER_NONE:
      default:
        break;
    }

  if (g_vg.navigating)
    {
      g_vg.navigating = false;
      printf("VelaGuard UI NAV: finished target=%s\n",
             vg_page_name(g_vg.current_page));
    }
}

static void vg_render_timer_cb(lv_timer_t *timer)
{
  vg_process_pending_render();
  lv_timer_pause(timer);
}

static void vg_set_font(lv_obj_t *obj)
{
#if VG_LIGHTWEIGHT_UI
  lv_obj_set_style_text_font(obj, LV_FONT_DEFAULT, LV_PART_MAIN);
#else
  lv_obj_set_style_text_font(obj, &velaguard_font_30, LV_PART_MAIN);
#endif
}

static lv_obj_t *vg_label(lv_obj_t *parent, const char *text, int32_t width,
                          lv_text_align_t align, uint32_t color)
{
  lv_obj_t *label = lv_label_create(parent);

  vg_set_font(label);
  lv_label_set_text(label, text);
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label, width);
  lv_obj_set_style_text_align(label, align, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);

  return label;
}

static lv_obj_t *vg_fixed_root(lv_obj_t *scr)
{
  lv_obj_t *root = lv_obj_create(scr);

  lv_obj_set_size(root, VG_SCREEN_W, VG_SCREEN_H);
  lv_obj_align(root, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(root, 0, LV_PART_MAIN);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

  return root;
}

static lv_obj_t *vg_image_at(lv_obj_t *parent,
                             const lv_image_dsc_t *src,
                             int32_t x, int32_t y)
{
  lv_obj_t *img = lv_image_create(parent);

  lv_image_set_src(img, src);
  lv_obj_align(img, LV_ALIGN_TOP_LEFT, x, y);

  return img;
}

static void vg_image_set_src_if_needed(lv_obj_t *img, int *current,
                                       int value,
                                       const lv_image_dsc_t * const *table)
{
  if (img == NULL || current == NULL || *current == value)
    {
      return;
    }

  lv_image_set_src(img, table[value]);
  *current = value;
}

static lv_obj_t *vg_round_band(lv_obj_t *parent, int32_t x, int32_t y,
                               int32_t w, int32_t h, uint32_t color,
                               const char *text)
{
  lv_obj_t *band;
  lv_obj_t *label;

  band = lv_obj_create(parent);
  lv_obj_set_size(band, w, h);
  lv_obj_align(band, LV_ALIGN_TOP_LEFT, x, y);
  lv_obj_set_style_bg_color(band, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_border_width(band, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(band, h / 2, LV_PART_MAIN);
  lv_obj_clear_flag(band, LV_OBJ_FLAG_SCROLLABLE);

  label = lv_label_create(band);
  vg_set_font(label);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, lv_color_hex(VG_COLOR_TEXT),
                              LV_PART_MAIN);
  lv_obj_center(label);

  return band;
}

static void vg_home_event_cb(lv_event_t *event)
{
  lv_event_code_t code = lv_event_get_code(event);
  lv_indev_t *indev;

  lv_event_stop_bubbling(event);

  if (code == LV_EVENT_PRESSED)
    {
      indev = lv_indev_active();
      g_vg.gesture_consumed = false;
      g_vg.home_press_start_ms = vg_uptime_ms();
      g_vg.home_press_moved = false;
      g_vg.home_edit_handled = false;
      if (indev != NULL)
        {
          lv_indev_get_point(indev, &g_vg.home_press_point);
        }
      printf("VelaGuard UI: home pressed\n");
      return;
    }
  else if (code == LV_EVENT_PRESSING)
    {
      lv_point_t point;

      indev = lv_indev_active();
      if (indev == NULL || g_vg.home_press_start_ms == 0)
        {
          return;
        }

      lv_indev_get_point(indev, &point);
      if (point.x - g_vg.home_press_point.x > VG_HOME_GESTURE_SLOP_PX ||
          g_vg.home_press_point.x - point.x > VG_HOME_GESTURE_SLOP_PX ||
          point.y - g_vg.home_press_point.y > VG_HOME_GESTURE_SLOP_PX ||
          g_vg.home_press_point.y - point.y > VG_HOME_GESTURE_SLOP_PX)
        {
          g_vg.home_press_moved = true;
        }
      else if (!g_vg.home_press_moved && !g_vg.home_edit_handled &&
               vg_uptime_ms() - g_vg.home_press_start_ms >=
               VG_HOME_EDIT_LONG_MS)
        {
          g_vg.home_edit_handled = true;
          printf("VelaGuard UI: watchface edit long press\n");
          vg_nav_request(VG_PAGE_WATCHFACE_PICKER, LV_DIR_NONE,
                         "home-long");
        }

      return;
    }
  else if (code == LV_EVENT_RELEASED)
    {
      g_vg.home_press_start_ms = 0;
      return;
    }
  else if (code == LV_EVENT_GESTURE)
    {
      g_vg.home_press_moved = true;
      g_vg.home_press_start_ms = 0;
      vg_navigation_gesture_cb(event);
    }
}

static void vg_bluetooth_event_cb(lv_event_t *event)
{
  lv_event_code_t code = lv_event_get_code(event);

  lv_event_stop_bubbling(event);

  if (code == LV_EVENT_PRESSED)
    {
      g_vg.gesture_consumed = false;
      printf("VelaGuard UI: bluetooth pressed\n");
      return;
    }

  if (code == LV_EVENT_GESTURE)
    {
      vg_navigation_gesture_cb(event);
    }
}

static void vg_navigation_gesture_cb(lv_event_t *event)
{
  lv_indev_t *indev;
  lv_dir_t dir;

  if (lv_event_get_code(event) != LV_EVENT_GESTURE)
    {
      return;
    }

  indev = lv_indev_active();
  dir = indev == NULL ? LV_DIR_NONE : lv_indev_get_gesture_dir(indev);
  if (dir != LV_DIR_LEFT && dir != LV_DIR_RIGHT)
    {
      printf("VelaGuard UI: gesture dir=%d ignored\n", (int)dir);
      return;
    }

  if (g_vg.current_page == VG_PAGE_HOME)
    {
      vg_nav_request(VG_PAGE_BLUETOOTH, dir, "page-gesture");
    }
  else if (g_vg.current_page == VG_PAGE_BLUETOOTH)
    {
      vg_nav_request(VG_PAGE_HOME, dir, "page-gesture");
    }
}

static void vg_ble_toggle_event_cb(lv_event_t *event)
{
  lv_event_code_t code = lv_event_get_code(event);
  bool target;
  uint64_t now;

  if (code != LV_EVENT_CLICKED)
    {
      return;
    }

  lv_event_stop_bubbling(event);
  if (vg_ble_has_pending_enable_request())
    {
      printf("VelaGuard UI: BLE toggle ignored while request pending\n");
      return;
    }

  now = vg_uptime_ms();
  if (g_vg.ble_toggle_last_ms != 0 &&
      now - g_vg.ble_toggle_last_ms < 500)
    {
      return;
    }

  g_vg.ble_toggle_last_ms = now;
  target = !vg_ble_is_enabled();
  vg_ble_request_set_enabled(target);
  printf("VelaGuard UI: BLE toggle request target=%d\n", target ? 1 : 0);
  vg_schedule_render(VG_RENDER_BLUETOOTH);
}

static lv_obj_t *vg_action_button(lv_obj_t *parent, const char *text,
                                  int32_t x, int32_t y, int32_t w,
                                  int32_t h, uint32_t color,
                                  enum vg_action_e action);

static const char *vg_page_name(enum vg_page_e page)
{
  switch (page)
    {
      case VG_PAGE_HOME:
        return "home";

      case VG_PAGE_BLUETOOTH:
        return "bluetooth";

      case VG_PAGE_WATCHFACE_PICKER:
        return "watchface";

      default:
        return "unknown";
    }
}

static void vg_nav_request(enum vg_page_e target, lv_dir_t dir,
                           const char *source)
{
  printf("VelaGuard UI NAV: request source=%s dir=%d current=%s target=%s busy=%d consumed=%d\n",
         source, (int)dir, vg_page_name(g_vg.current_page),
         vg_page_name(target), g_vg.navigating ? 1 : 0,
         g_vg.gesture_consumed ? 1 : 0);

  if (g_vg.navigating || g_vg.gesture_consumed ||
      target == g_vg.current_page)
    {
      printf("VelaGuard UI NAV: dropped source=%s\n", source);
      return;
    }

  g_vg.navigating = true;
  g_vg.gesture_consumed = true;
  g_vg.target_page = target;
  printf("VelaGuard UI NAV: accepted %s -> %s\n",
         vg_page_name(g_vg.current_page), vg_page_name(target));

  switch (target)
    {
      case VG_PAGE_BLUETOOTH:
        vg_schedule_render(VG_RENDER_BLUETOOTH);
        break;

      case VG_PAGE_WATCHFACE_PICKER:
        vg_schedule_render(VG_RENDER_WATCHFACE);
        break;

      case VG_PAGE_HOME:
      default:
        vg_schedule_render(VG_RENDER_HOME);
        break;
    }
}

static void vg_action_cb(lv_event_t *event)
{
  enum vg_action_e action;

  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
    {
      return;
    }

  action = (enum vg_action_e)(uintptr_t)lv_event_get_user_data(event);
  printf("VelaGuard UI: click action=%d\n", (int)action);

  switch (action)
    {
      case VG_ACTION_SOS:
        vg_trigger_event(VG_EVENT_MANUAL_SOS);
        break;

      case VG_ACTION_DEMO_FALL:
        vg_trigger_event(VG_EVENT_FALL);
        break;

      case VG_ACTION_DEMO_SOUND:
        vg_trigger_event(VG_EVENT_SOUND);
        break;

      case VG_ACTION_DEMO_VOICE:
        vg_trigger_event(VG_EVENT_VOICE);
        break;

      case VG_ACTION_CANCEL:
        vg_cancel_event();
        break;

      case VG_ACTION_CONFIRM:
        vg_confirm_alert();
        break;

      case VG_ACTION_RESOLVE:
        vg_resolve_event();
        break;

      case VG_ACTION_HISTORY:
        vg_schedule_render(VG_RENDER_HISTORY);
        break;

      case VG_ACTION_SETTINGS:
        vg_schedule_render(VG_RENDER_SETTINGS);
        break;

      case VG_ACTION_BLUETOOTH:
        vg_schedule_render(VG_RENDER_BLUETOOTH);
        break;

      case VG_ACTION_BLE_TOGGLE:
        vg_ble_request_set_enabled(!vg_ble_is_enabled());
        vg_schedule_render(VG_RENDER_BLUETOOTH);
        break;

      case VG_ACTION_FALL_CANCEL_HOLD:
      case VG_ACTION_FALL_CONFIRM_HOLD:
        break;

      case VG_ACTION_DIAL_RAINBOW:
        g_vg.watchface = 0;
        vg_schedule_render(VG_RENDER_HOME);
        break;

      case VG_ACTION_DIAL_SIMPLE:
        g_vg.watchface = 1;
        vg_schedule_render(VG_RENDER_HOME);
        break;

      case VG_ACTION_MODE:
        g_vg.mode = (g_vg.mode + 1) % 4;
        vg_schedule_render(VG_RENDER_SETTINGS);
        break;

      case VG_ACTION_BACK:
      default:
        vg_schedule_render(VG_RENDER_CURRENT);
        break;
    }
}

static lv_obj_t *vg_action_button(lv_obj_t *parent, const char *text,
                                  int32_t x, int32_t y, int32_t w,
                                  int32_t h, uint32_t color,
                                  enum vg_action_e action)
{
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_t *label;

  lv_obj_set_size(btn, w, h);
  lv_obj_align(btn, LV_ALIGN_TOP_LEFT, x, y);
  lv_obj_set_style_bg_color(btn, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
  lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
  vg_set_font(btn);
  lv_obj_add_event_cb(btn, vg_action_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)action);

  label = lv_label_create(btn);
  vg_set_font(label);
  lv_label_set_text(label, text);
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label, w - 8);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, lv_color_hex(VG_COLOR_TEXT),
                              LV_PART_MAIN);
  lv_obj_center(label);

  return btn;
}

static void vg_hold_button_cb(lv_event_t *event)
{
  enum vg_action_e action;
  lv_event_code_t code = lv_event_get_code(event);

  action = (enum vg_action_e)(uintptr_t)lv_event_get_user_data(event);

  if (code == LV_EVENT_PRESSED)
    {
      if (action == VG_ACTION_FALL_CANCEL_HOLD)
        {
          g_vg.hold_cancel_start_ms = vg_uptime_ms();
          g_vg.hold_cancel_active = true;
          g_vg.hold_action = action;
          g_vg.hold_last_value = -1;
          vg_render_fall_hold_progress(action);
        }
      else if (action == VG_ACTION_FALL_CONFIRM_HOLD)
        {
          g_vg.hold_confirm_start_ms = vg_uptime_ms();
          g_vg.hold_confirm_active = true;
          g_vg.hold_action = action;
          g_vg.hold_last_value = -1;
          vg_render_fall_hold_progress(action);
        }

      lv_event_stop_bubbling(event);
    }
  else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST)
    {
      uint64_t now = vg_uptime_ms();
      bool completed = false;

      if (action == VG_ACTION_FALL_CANCEL_HOLD)
        {
          completed = g_vg.hold_cancel_active &&
                      now - g_vg.hold_cancel_start_ms >=
                      VG_HOLD_CONFIRM_MS;
          g_vg.hold_cancel_active = false;
        }
      else if (action == VG_ACTION_FALL_CONFIRM_HOLD)
        {
          completed = g_vg.hold_confirm_active &&
                      now - g_vg.hold_confirm_start_ms >=
                      VG_HOLD_CONFIRM_MS;
          g_vg.hold_confirm_active = false;
        }

      g_vg.hold_action = 0;
      g_vg.hold_last_value = -1;
      if (completed)
        {
          if (action == VG_ACTION_FALL_CANCEL_HOLD)
            {
              vg_cancel_event();
            }
          else if (action == VG_ACTION_FALL_CONFIRM_HOLD)
            {
              vg_confirm_alert();
            }

          lv_event_stop_bubbling(event);
          return;
        }

      if (g_vg.state == VG_STATE_PREALERT &&
          g_vg.active.type == VG_EVENT_FALL &&
          g_vg.hold_overlay != NULL)
        {
          vg_schedule_render(VG_RENDER_ALERT);
        }

      lv_event_stop_bubbling(event);
    }
}

static lv_obj_t *vg_hold_button(lv_obj_t *parent, const char *text,
                                int32_t x, int32_t y, int32_t w,
                                int32_t h, uint32_t bg, uint32_t fg,
                                enum vg_action_e action)
{
  lv_obj_t *btn;
  lv_obj_t *label;

  btn = lv_button_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_align(btn, LV_ALIGN_TOP_LEFT, x, y);
  lv_obj_set_style_bg_color(btn, lv_color_hex(bg), LV_PART_MAIN);
  lv_obj_set_style_radius(btn, 2, LV_PART_MAIN);
  lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
  lv_obj_add_event_cb(btn, vg_hold_button_cb, LV_EVENT_PRESSED,
                      (void *)(uintptr_t)action);
  lv_obj_add_event_cb(btn, vg_hold_button_cb, LV_EVENT_RELEASED,
                      (void *)(uintptr_t)action);
  lv_obj_add_event_cb(btn, vg_hold_button_cb, LV_EVENT_PRESS_LOST,
                      (void *)(uintptr_t)action);

  label = lv_label_create(btn);
  vg_set_font(label);
  lv_label_set_text(label, text);
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label, w - 8);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, lv_color_hex(fg), LV_PART_MAIN);
  lv_obj_center(label);

  return btn;
}

static void vg_render_header(lv_obj_t *scr, const char *status,
                             uint32_t status_color)
{
  int32_t w = lv_display_get_horizontal_resolution(NULL);
  lv_obj_t *title;
  lv_obj_t *chip;
  lv_obj_t *chip_label;

  title = vg_label(scr, "VelaGuard 安全守护", w - 24, LV_TEXT_ALIGN_LEFT,
                   VG_COLOR_TEXT);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 12, 12);

  chip = lv_obj_create(scr);
  lv_obj_set_size(chip, 104, 30);
  lv_obj_align(chip, LV_ALIGN_TOP_RIGHT, -12, 10);
  lv_obj_set_style_bg_color(chip, lv_color_hex(status_color), LV_PART_MAIN);
  lv_obj_set_style_border_width(chip, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(chip, 15, LV_PART_MAIN);
  lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);

  chip_label = lv_label_create(chip);
  vg_set_font(chip_label);
  lv_label_set_text(chip_label, status);
  lv_obj_set_style_text_color(chip_label, lv_color_hex(VG_COLOR_TEXT),
                              LV_PART_MAIN);
  lv_obj_center(chip_label);
}

static void vg_update_watchface_time(void)
{
  time_t now;
  time_t display_now;
  struct tm tm_now;
  int hour;
  int minute;
  int month;
  int mday;
  int wday;
  int time_digits[4];
  int date_digits[4];

  if (g_vg.home_time_img[0] == NULL)
    {
      return;
    }

  now = time(NULL);
  display_now = now + VG_LOCAL_TIME_OFFSET_SECONDS;
  if (gmtime_r(&display_now, &tm_now) == NULL)
    {
      return;
    }

  if (g_vg.home_last_minute == tm_now.tm_min &&
      g_vg.home_last_mday == tm_now.tm_mday)
    {
      return;
    }

  hour = tm_now.tm_hour;
  minute = tm_now.tm_min;
  month = tm_now.tm_mon + 1;
  mday = tm_now.tm_mday;
  wday = tm_now.tm_wday;
  if (wday < 0 || wday > 6)
    {
      wday = 0;
    }

  time_digits[0] = hour / 10;
  time_digits[1] = hour % 10;
  time_digits[2] = minute / 10;
  time_digits[3] = minute % 10;
  date_digits[0] = month / 10;
  date_digits[1] = month % 10;
  date_digits[2] = mday / 10;
  date_digits[3] = mday % 10;

  printf("VelaGuard UI: watchface update time=%d%d:%d%d date=%d%d/%d%d week=%d\n",
         time_digits[0], time_digits[1], time_digits[2], time_digits[3],
         date_digits[0], date_digits[1], date_digits[2], date_digits[3],
         wday);

#if VG_LIGHTWEIGHT_UI
  if (g_vg.home_date_label != NULL)
    {
      char time_line[8];

      snprintf(time_line, sizeof(time_line), "%02d:%02d", hour, minute);
      lv_label_set_text(g_vg.home_date_label, time_line);
    }

  if (g_vg.home_week_label != NULL)
    {
      char date_line[16];

      snprintf(date_line, sizeof(date_line), "%02d/%02d W%d", month, mday,
               wday);
      lv_label_set_text(g_vg.home_week_label, date_line);
    }
#else
  if (g_vg.watchface == 0)
    {
      if (g_vg.home_time_img[1] == NULL ||
          g_vg.home_time_img[2] == NULL ||
          g_vg.home_time_img[3] == NULL)
        {
          return;
        }

      vg_image_set_src_if_needed(g_vg.home_time_img[0],
                                 &g_vg.home_time_digit[0], time_digits[0],
                                 g_rainbow_white_digits);
      vg_image_set_src_if_needed(g_vg.home_time_img[1],
                                 &g_vg.home_time_digit[1], time_digits[1],
                                 g_rainbow_white_digits);
      vg_image_set_src_if_needed(g_vg.home_time_img[2],
                                 &g_vg.home_time_digit[2], time_digits[2],
                                 g_rainbow_blue_digits);
      vg_image_set_src_if_needed(g_vg.home_time_img[3],
                                 &g_vg.home_time_digit[3], time_digits[3],
                                 g_rainbow_blue_digits);
      if (g_vg.home_date_label != NULL)
        {
          char date_line[8];

          snprintf(date_line, sizeof(date_line), "%02d/%02d", month, mday);
          lv_label_set_text(g_vg.home_date_label, date_line);
        }

      if (g_vg.home_week_label != NULL)
        {
          lv_label_set_text(g_vg.home_week_label, g_week_names[wday]);
        }
    }
  else
    {
      if (g_vg.home_time_img[1] == NULL ||
          g_vg.home_time_img[2] == NULL ||
          g_vg.home_time_img[3] == NULL ||
          g_vg.home_date_img[0] == NULL ||
          g_vg.home_date_img[1] == NULL ||
          g_vg.home_date_img[2] == NULL ||
          g_vg.home_date_img[3] == NULL ||
          g_vg.home_week_img == NULL)
        {
          return;
        }

      vg_image_set_src_if_needed(g_vg.home_time_img[0],
                                 &g_vg.home_time_digit[0], time_digits[0],
                                 g_touch_hour_digits);
      vg_image_set_src_if_needed(g_vg.home_time_img[1],
                                 &g_vg.home_time_digit[1], time_digits[1],
                                 g_touch_hour_digits);
      vg_image_set_src_if_needed(g_vg.home_time_img[2],
                                 &g_vg.home_time_digit[2], time_digits[2],
                                 g_touch_hour_digits);
      vg_image_set_src_if_needed(g_vg.home_time_img[3],
                                 &g_vg.home_time_digit[3], time_digits[3],
                                 g_touch_hour_digits);

      vg_image_set_src_if_needed(g_vg.home_date_img[0],
                                 &g_vg.home_date_digit[0], date_digits[0],
                                 g_touch_date_digits);
      vg_image_set_src_if_needed(g_vg.home_date_img[1],
                                 &g_vg.home_date_digit[1], date_digits[1],
                                 g_touch_date_digits);
      vg_image_set_src_if_needed(g_vg.home_date_img[2],
                                 &g_vg.home_date_digit[2], date_digits[2],
                                 g_touch_date_digits);
      vg_image_set_src_if_needed(g_vg.home_date_img[3],
                                 &g_vg.home_date_digit[3], date_digits[3],
                                 g_touch_date_digits);
      vg_image_set_src_if_needed(g_vg.home_week_img, &g_vg.home_week_digit,
                                 wday, g_touch_week_digits);
    }
#endif

  g_vg.home_last_minute = tm_now.tm_min;
  g_vg.home_last_mday = tm_now.tm_mday;
}

static void vg_update_countdown_visuals(void)
{
  char line[16];
  int value = g_vg.countdown;

  if (value < 0)
    {
      value = 0;
    }

  if (g_vg.countdown_single_img != NULL)
    {
      lv_image_set_src(g_vg.countdown_single_img,
                       g_count_digits[value % 10]);
    }

  if (g_vg.countdown_tens_img != NULL &&
      g_vg.countdown_ones_img != NULL)
    {
      lv_image_set_src(g_vg.countdown_tens_img,
                       g_touch_hour_digits[(value / 10) % 10]);
      lv_image_set_src(g_vg.countdown_ones_img,
                       g_touch_hour_digits[value % 10]);
      if (value < 10)
        {
          lv_obj_add_flag(g_vg.countdown_tens_img, LV_OBJ_FLAG_HIDDEN);
          lv_obj_align(g_vg.countdown_ones_img, LV_ALIGN_TOP_LEFT,
                       VG_X(146), VG_Y(104));
        }
      else
        {
          lv_obj_clear_flag(g_vg.countdown_tens_img, LV_OBJ_FLAG_HIDDEN);
          lv_obj_align(g_vg.countdown_ones_img, LV_ALIGN_TOP_LEFT,
                       VG_X(160), VG_Y(104));
        }
    }

  if (g_vg.countdown_label != NULL)
    {
      snprintf(line, sizeof(line), "%d秒", value);
      lv_label_set_text(g_vg.countdown_label, line);
    }
}

static void vg_render_fall_hold_progress(enum vg_action_e action)
{
  lv_obj_t *scr = lv_screen_active();
  lv_obj_t *root;
  lv_obj_t *circle;
  lv_obj_t *label;
  uint32_t indicator_color =
    action == VG_ACTION_FALL_CANCEL_HOLD ? 0xffd447 : 0xff6a22;
  uint32_t band_color =
    action == VG_ACTION_FALL_CANCEL_HOLD ? 0x6c5561 : 0x68424d;
  const char *band_text =
    action == VG_ACTION_FALL_CANCEL_HOLD ? "我没事" : "正在求助中";

  if (g_vg.hold_overlay != NULL)
    {
      lv_obj_delete(g_vg.hold_overlay);
      g_vg.hold_overlay = NULL;
    }

  root = lv_obj_create(scr);
  g_vg.hold_overlay = root;
  lv_obj_set_size(root, VG_SCREEN_W, VG_SCREEN_H);
  lv_obj_align(root, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(root, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(root, 0, LV_PART_MAIN);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_CLICKABLE);

  vg_image_at(root, &velaguard_img_bg_alarm, 0, 0);

  circle = lv_obj_create(root);
  lv_obj_set_size(circle, VG_X(132), VG_X(132));
  lv_obj_align(circle, LV_ALIGN_TOP_MID, 0, VG_Y(46));
  lv_obj_set_style_bg_color(circle, lv_color_hex(0xa90f20), LV_PART_MAIN);
  lv_obj_set_style_radius(circle, VG_X(66), LV_PART_MAIN);
  lv_obj_set_style_border_width(circle, 0, LV_PART_MAIN);
  lv_obj_clear_flag(circle, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(circle, LV_OBJ_FLAG_CLICKABLE);

  g_vg.hold_arc = lv_arc_create(root);
  lv_obj_set_size(g_vg.hold_arc, VG_X(142), VG_X(142));
  lv_obj_align(g_vg.hold_arc, LV_ALIGN_TOP_MID, 0, VG_Y(41));
  lv_arc_set_range(g_vg.hold_arc, 0, 100);
  lv_arc_set_value(g_vg.hold_arc, 0);
  lv_arc_set_bg_angles(g_vg.hold_arc, 0, 360);
  lv_arc_set_rotation(g_vg.hold_arc, 270);
  lv_obj_remove_style(g_vg.hold_arc, NULL, LV_PART_KNOB);
  lv_obj_clear_flag(g_vg.hold_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_color(g_vg.hold_arc, lv_color_hex(indicator_color),
                             LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(g_vg.hold_arc, VG_X(7), LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(g_vg.hold_arc, lv_color_hex(0x5d0614),
                             LV_PART_MAIN);
  lv_obj_set_style_arc_width(g_vg.hold_arc, VG_X(7), LV_PART_MAIN);

  g_vg.hold_digit_img = vg_image_at(root, g_count_digits[3], 0, 0);
  lv_obj_align(g_vg.hold_digit_img, LV_ALIGN_TOP_MID, 0, VG_Y(59));

  label = vg_label(root, "请勿松开", VG_X(170), LV_TEXT_ALIGN_CENTER,
                   VG_COLOR_TEXT);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, VG_Y(121));

  vg_round_band(root, VG_X(45), VG_Y(180), VG_X(150), VG_Y(38),
                band_color, band_text);
  lv_obj_move_foreground(root);
  vg_update_hold_progress_visuals(vg_uptime_ms());
}

static void vg_update_hold_progress_visuals(uint64_t now)
{
  uint64_t start_ms = 0;
  uint64_t elapsed;
  int value;
  int remain;

  if (!g_vg.hold_cancel_active && !g_vg.hold_confirm_active)
    {
      return;
    }

  start_ms = g_vg.hold_cancel_active ? g_vg.hold_cancel_start_ms :
             g_vg.hold_confirm_start_ms;
  if (start_ms == 0)
    {
      return;
    }

  elapsed = now - start_ms;
  if (elapsed > VG_HOLD_CONFIRM_MS)
    {
      elapsed = VG_HOLD_CONFIRM_MS;
    }

  value = (int)(elapsed * 100 / VG_HOLD_CONFIRM_MS);
  if (g_vg.hold_arc != NULL)
    {
      lv_arc_set_value(g_vg.hold_arc, value);
    }

  remain = (int)((VG_HOLD_CONFIRM_MS - elapsed + 999) / 1000);
  if (remain < 1)
    {
      remain = 1;
    }
  else if (remain > 9)
    {
      remain = 9;
    }

  if (g_vg.hold_digit_img != NULL && remain != g_vg.hold_last_value)
    {
      g_vg.hold_last_value = remain;
      lv_image_set_src(g_vg.hold_digit_img, g_count_digits[remain]);
    }
}

static void vg_update_alarm_hold(void)
{
  uint64_t now;

  if (g_vg.state != VG_STATE_PREALERT ||
      g_vg.active.type != VG_EVENT_FALL)
    {
      g_vg.hold_cancel_active = false;
      g_vg.hold_confirm_active = false;
      g_vg.hold_action = 0;
      g_vg.hold_last_value = -1;
      return;
    }

  now = vg_uptime_ms();
  vg_update_hold_progress_visuals(now);

  if (g_vg.hold_cancel_active &&
      now - g_vg.hold_cancel_start_ms >= VG_HOLD_CONFIRM_MS)
    {
      g_vg.hold_cancel_active = false;
      g_vg.hold_action = 0;
      vg_cancel_event();
      return;
    }

  if (g_vg.hold_confirm_active &&
      now - g_vg.hold_confirm_start_ms >= VG_HOLD_CONFIRM_MS)
    {
      g_vg.hold_confirm_active = false;
      g_vg.hold_action = 0;
      vg_confirm_alert();
    }
}

static void vg_render_home(void)
{
  lv_obj_t *scr;
  lv_obj_t *root;
#if VG_LIGHTWEIGHT_UI
  lv_obj_t *panel;
#endif

  /* Home and Bluetooth are the normal gesture pair.  Reusing their object
   * trees avoids freeing LVGL allocations while the device is active. */
  if (g_vg.home_root != NULL && g_vg.bluetooth_root != NULL)
    {
      g_vg.current_page = VG_PAGE_HOME;
      lv_obj_clear_flag(g_vg.home_root, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(g_vg.bluetooth_root, LV_OBJ_FLAG_HIDDEN);
      vg_update_watchface_time();
      return;
    }

  scr = vg_screen_reset();
  g_vg.current_page = VG_PAGE_HOME;
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_add_event_cb(scr, vg_navigation_gesture_cb, LV_EVENT_GESTURE,
                      NULL);

  root = vg_fixed_root(scr);
  g_vg.home_root = root;
  lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(root, vg_home_event_cb, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(root, vg_home_event_cb, LV_EVENT_PRESSING, NULL);
  lv_obj_add_event_cb(root, vg_home_event_cb, LV_EVENT_RELEASED, NULL);
  lv_obj_add_event_cb(root, vg_home_event_cb, LV_EVENT_GESTURE, NULL);

  g_vg.home_last_minute = -1;
  g_vg.home_last_mday = -1;

#if VG_LIGHTWEIGHT_UI
  lv_obj_set_style_bg_color(root, lv_color_hex(0x101820), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);

  panel = lv_obj_create(root);
  lv_obj_set_size(panel, VG_X(188), VG_Y(104));
  lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, VG_Y(58));
  lv_obj_set_style_bg_color(panel, lv_color_hex(0x1f2f3d), LV_PART_MAIN);
  lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(panel, VG_X(10), LV_PART_MAIN);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  g_vg.home_date_label = vg_label(panel, "--:--", VG_X(160),
                                  LV_TEXT_ALIGN_CENTER, VG_COLOR_TEXT);
  lv_obj_set_style_text_font(g_vg.home_date_label, LV_FONT_DEFAULT,
                             LV_PART_MAIN);
  lv_obj_align(g_vg.home_date_label, LV_ALIGN_TOP_MID, 0, VG_Y(20));

  g_vg.home_week_label = vg_label(panel, "--/--", VG_X(160),
                                  LV_TEXT_ALIGN_CENTER, VG_COLOR_MUTED);
  lv_obj_align(g_vg.home_week_label, LV_ALIGN_TOP_MID, 0, VG_Y(60));

  vg_round_band(root, VG_X(40), VG_Y(188), VG_X(160), VG_Y(40),
                vg_ble_is_connected() ? VG_COLOR_OK : VG_COLOR_INFO,
                vg_ble_is_connected() ? "BLE connected" : "VelaGuard");
#else
  if (g_vg.watchface == 0)
    {
      vg_image_at(root, &velaguard_img_icon_rainbow_rain_bg, 0, 0);
      g_vg.home_time_img[0] = vg_image_at(root,
        &velaguard_img_icon_rainbow_rain_white_0, VG_X(64), VG_Y(49));
      g_vg.home_time_img[1] = vg_image_at(root,
        &velaguard_img_icon_rainbow_rain_white_0, VG_X(122), VG_Y(48));
      g_vg.home_time_img[2] = vg_image_at(root,
        &velaguard_img_icon_rainbow_rain_blue_0, VG_X(64), VG_Y(129));
      g_vg.home_time_img[3] = vg_image_at(root,
        &velaguard_img_icon_rainbow_rain_blue_0, VG_X(122), VG_Y(128));
      g_vg.home_date_label = vg_label(root, "", VG_X(96),
                                      LV_TEXT_ALIGN_LEFT, VG_COLOR_TEXT);
      lv_label_set_long_mode(g_vg.home_date_label, LV_LABEL_LONG_CLIP);
      lv_obj_align(g_vg.home_date_label, LV_ALIGN_TOP_LEFT,
                   VG_X(42), VG_Y(241));
      g_vg.home_week_label = vg_label(root, "", VG_X(60),
                                      LV_TEXT_ALIGN_LEFT, VG_COLOR_TEXT);
      lv_obj_align(g_vg.home_week_label, LV_ALIGN_TOP_LEFT,
                   VG_X(112), VG_Y(241));
      vg_image_at(root, &velaguard_img_icon_rainbow_rain_battery_5,
                  VG_X(163), VG_Y(247));
    }
  else
    {
      vg_image_at(root, &velaguard_img_icon_touch_future_illustration,
                  VG_X(18), VG_Y(36));
      g_vg.home_time_img[0] = vg_image_at(root,
        &velaguard_img_icon_touch_future_hour_0, VG_X(36), VG_Y(205));
      g_vg.home_time_img[1] = vg_image_at(root,
        &velaguard_img_icon_touch_future_hour_0, VG_X(72), VG_Y(205));
      vg_image_at(root, &velaguard_img_icon_touch_future_colon,
                  VG_X(108), VG_Y(205));
      g_vg.home_time_img[2] = vg_image_at(root,
        &velaguard_img_icon_touch_future_hour_0, VG_X(136), VG_Y(205));
      g_vg.home_time_img[3] = vg_image_at(root,
        &velaguard_img_icon_touch_future_hour_0, VG_X(172), VG_Y(205));
      g_vg.home_date_img[0] = vg_image_at(root,
        &velaguard_img_icon_touch_future_date_0, VG_X(31), VG_Y(249));
      g_vg.home_date_img[1] = vg_image_at(root,
        &velaguard_img_icon_touch_future_date_0, VG_X(48), VG_Y(249));
      vg_image_at(root, &velaguard_img_icon_touch_future_date_slash,
                  VG_X(65), VG_Y(249));
      g_vg.home_date_img[2] = vg_image_at(root,
        &velaguard_img_icon_touch_future_date_0, VG_X(74), VG_Y(249));
      g_vg.home_date_img[3] = vg_image_at(root,
        &velaguard_img_icon_touch_future_date_0, VG_X(91), VG_Y(249));
      g_vg.home_week_img = vg_image_at(root,
        &velaguard_img_icon_touch_future_week_0, VG_X(110), VG_Y(250));
      vg_image_at(root, &velaguard_img_icon_touch_future_battery_5,
                  VG_X(184), VG_Y(248));
    }
#endif

  vg_update_watchface_time();
  vg_create_bluetooth_page(scr);
}

static void vg_render_prealert(void)
{
  vg_render_alert();
}

static void vg_render_alert(void)
{
  lv_obj_t *scr = vg_screen_reset();
  lv_obj_t *root;
  lv_obj_t *img;
  lv_obj_t *label;
  lv_obj_t *btn;
  bool prealert = g_vg.state == VG_STATE_PREALERT;
  bool fall = g_vg.active.type == VG_EVENT_FALL;

  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);
  root = vg_fixed_root(scr);
  vg_image_at(root, &velaguard_img_bg_alarm, 0, 0);

  if (prealert && g_vg.active.type == VG_EVENT_MANUAL_SOS)
    {
      printf("VelaGuard UI: SOS countdown render value=%d total=%d\n",
             g_vg.countdown, g_vg.countdown_total);
      g_vg.countdown_arc = lv_arc_create(root);
      lv_obj_set_size(g_vg.countdown_arc, VG_X(86), VG_X(86));
      lv_obj_align(g_vg.countdown_arc, LV_ALIGN_TOP_MID, 0, VG_Y(34));
      lv_arc_set_range(g_vg.countdown_arc, 0, 100);
      lv_arc_set_value(g_vg.countdown_arc, 100);
      lv_arc_set_bg_angles(g_vg.countdown_arc, 0, 360);
      lv_arc_set_rotation(g_vg.countdown_arc, 270);
      lv_obj_remove_style(g_vg.countdown_arc, NULL, LV_PART_KNOB);
      lv_obj_clear_flag(g_vg.countdown_arc, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_arc_color(g_vg.countdown_arc,
                                 lv_color_hex(0xff7a20),
                                 LV_PART_INDICATOR);
      lv_obj_set_style_arc_width(g_vg.countdown_arc, VG_X(4),
                                 LV_PART_INDICATOR);
      lv_obj_set_style_arc_color(g_vg.countdown_arc,
                                 lv_color_hex(0x8d1515), LV_PART_MAIN);
      lv_obj_set_style_arc_width(g_vg.countdown_arc, VG_X(4),
                                 LV_PART_MAIN);

      g_vg.countdown_single_img = vg_image_at(root,
        g_count_digits[g_vg.countdown % 10], VG_X(98), VG_Y(42));
      lv_obj_align(g_vg.countdown_single_img, LV_ALIGN_TOP_MID, 0,
                   VG_Y(47));

      label = vg_label(root, "请勿松开", VG_X(180), LV_TEXT_ALIGN_CENTER,
                       VG_COLOR_TEXT);
      lv_obj_align(label, LV_ALIGN_TOP_MID, 0, VG_Y(122));
      vg_round_band(root, VG_X(45), VG_Y(197), VG_X(150), VG_Y(48),
                    0x68424d, "正在求助中");
      vg_update_manual_sos_progress(vg_uptime_ms());
      return;
    }

  if (prealert && fall)
    {
      label = vg_label(root, "检测到跌倒", VG_X(220), LV_TEXT_ALIGN_CENTER,
                       VG_COLOR_TEXT);
      lv_obj_align(label, LV_ALIGN_TOP_MID, 0, VG_Y(42));

      img = vg_image_at(root, &velaguard_img_fall_icon, VG_X(43), VG_Y(88));
      lv_obj_move_foreground(img);

      g_vg.countdown_tens_img = vg_image_at(root,
        g_touch_hour_digits[(g_vg.countdown / 10) % 10], VG_X(126),
        VG_Y(104));
      g_vg.countdown_ones_img = vg_image_at(root,
        g_touch_hour_digits[g_vg.countdown % 10], VG_X(160), VG_Y(104));
      label = vg_label(root, "秒", VG_X(28), LV_TEXT_ALIGN_CENTER,
                       VG_COLOR_TEXT);
      lv_obj_align(label, LV_ALIGN_TOP_LEFT, VG_X(198), VG_Y(109));

      vg_hold_button(root, "我没事\n长按", VG_X(21), VG_Y(211),
                     VG_X(90), VG_Y(54),
                     0xd79500, VG_COLOR_TEXT,
                     VG_ACTION_FALL_CANCEL_HOLD);
      vg_hold_button(root, "立即求助\n长按", VG_X(129), VG_Y(211),
                     VG_X(90), VG_Y(54),
                     0xff4b22, VG_COLOR_TEXT,
                     VG_ACTION_FALL_CONFIRM_HOLD);
      vg_update_countdown_visuals();
      return;
    }

  img = vg_image_at(root, fall ? &velaguard_img_fall_icon :
                    g_alarm_frames[0], fall ? VG_X(70) : VG_X(93),
                    fall ? VG_Y(62) : VG_Y(48));
  lv_obj_move_foreground(img);

  label = vg_label(root, fall ? "报警已上报" : "紧急求助已发送",
                   VG_X(220), LV_TEXT_ALIGN_CENTER, VG_COLOR_TEXT);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, VG_Y(142));

  label = vg_label(root, "请等待救援", VG_X(220), LV_TEXT_ALIGN_CENTER,
                   VG_COLOR_TEXT);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, VG_Y(176));

  btn = vg_action_button(root, "返回表盘", VG_X(36), VG_Y(218),
                         VG_X(168), VG_Y(42), 0xffffff, VG_ACTION_RESOLVE);
  lv_obj_set_style_text_color(lv_obj_get_child(btn, 0),
                              lv_color_hex(0xff2f00), LV_PART_MAIN);
}

static void vg_render_history(void)
{
  lv_obj_t *scr = vg_screen_reset();
  lv_obj_t *label;
  char line[224];
  int32_t w = lv_display_get_horizontal_resolution(NULL);
  int i;

  vg_render_header(scr, "记录", VG_COLOR_INFO);

  if (g_vg.history_count == 0)
    {
      label = vg_label(scr, "暂无事件", w - 24, LV_TEXT_ALIGN_CENTER,
                       VG_COLOR_MUTED);
      lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 160);
    }

  for (i = 0; i < g_vg.history_count; i++)
    {
      int index = g_vg.history_head - 1 - i;
      const struct vg_event_s *event;

      if (index < 0)
        {
          index += VG_HISTORY_SIZE;
        }

      event = &g_vg.history[index];
      snprintf(line, sizeof(line), "#%" PRIu32 " %s %s R%d C%d%%\n%s",
               event->id, vg_phase_title(event->phase),
               vg_event_title(event->type),
               event->risk, event->confidence, event->summary);

      label = vg_label(scr, line, w - 28, LV_TEXT_ALIGN_LEFT,
                       i == 0 ? VG_COLOR_TEXT : VG_COLOR_MUTED);
      lv_obj_align(label, LV_ALIGN_TOP_LEFT, 14, 64 + i * 62);
    }

  vg_action_button(scr, "返回", 12, 400, w - 24, 40, VG_COLOR_CARD_ALT,
                   VG_ACTION_BACK);
}

static void vg_render_settings(void)
{
  lv_obj_t *scr = vg_screen_reset();
  lv_obj_t *label;
  char line[128];
  int32_t w = lv_display_get_horizontal_resolution(NULL);

  vg_render_header(scr, "设置", VG_COLOR_INFO);

  snprintf(line, sizeof(line), "场景模式：%s", vg_mode_name(g_vg.mode));
  label = vg_label(scr, line, w - 24, LV_TEXT_ALIGN_CENTER, VG_COLOR_TEXT);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 84);

  label = vg_label(scr, "IMU 跌倒检测运行中",
                   w - 24, LV_TEXT_ALIGN_CENTER, VG_COLOR_MUTED);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 132);

  label = vg_label(scr, "输出：串口 JSON 事件", w - 24,
                   LV_TEXT_ALIGN_CENTER, VG_COLOR_MUTED);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 178);

  vg_action_button(scr, "下一模式", 12, 318, w - 24, 58, VG_COLOR_INFO,
                   VG_ACTION_MODE);
  vg_action_button(scr, "返回", 12, 392, w - 24, 38, VG_COLOR_CARD_ALT,
                   VG_ACTION_BACK);
}

static void vg_create_bluetooth_page(lv_obj_t *scr)
{
  lv_obj_t *root;
  lv_obj_t *label;
  lv_obj_t *sw;
  lv_obj_t *dot;
  char name[64];
  char addr[40];
  bool ble_switch_on;

  root = vg_fixed_root(scr);
  g_vg.bluetooth_root = root;
  lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(root, vg_bluetooth_event_cb, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(root, vg_bluetooth_event_cb, LV_EVENT_GESTURE, NULL);

  vg_image_at(root, &velaguard_img_ble_icon, VG_X(18), VG_Y(31));
  label = vg_label(root, "蓝牙", VG_X(110), LV_TEXT_ALIGN_LEFT,
                   VG_COLOR_TEXT);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, VG_X(58), VG_Y(32));

  sw = lv_button_create(root);
  ble_switch_on = vg_ble_is_enabled();
  lv_obj_set_size(sw, VG_X(96), VG_Y(54));
  lv_obj_align(sw, LV_ALIGN_TOP_RIGHT, -VG_X(8), VG_Y(15));
  lv_obj_set_style_bg_color(sw, lv_color_hex(ble_switch_on ?
                          0xff6a00 : 0x3a3a3a), LV_PART_MAIN);
  lv_obj_set_style_radius(sw, VG_Y(27), LV_PART_MAIN);
  lv_obj_set_style_border_width(sw, 0, LV_PART_MAIN);
  lv_obj_add_flag(sw, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(sw, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(sw, vg_ble_toggle_event_cb, LV_EVENT_CLICKED, NULL);

  dot = lv_obj_create(sw);
  lv_obj_set_size(dot, VG_X(28), VG_X(28));
  lv_obj_align(dot, ble_switch_on ? LV_ALIGN_RIGHT_MID :
               LV_ALIGN_LEFT_MID, ble_switch_on ? -VG_X(6) :
               VG_X(6), 0);
  lv_obj_set_style_bg_color(dot, lv_color_hex(0xffffff), LV_PART_MAIN);
  lv_obj_set_style_radius(dot, VG_X(12), LV_PART_MAIN);
  lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
  lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);

  vg_ble_get_local_address(addr, sizeof(addr));
  if (strcmp(addr, "initializing") == 0)
    {
      strlcpy(addr, "蓝牙初始化中", sizeof(addr));
    }
  else if (strcmp(addr, "pending") == 0)
    {
      strlcpy(addr, "地址未就绪", sizeof(addr));
  }

  vg_ble_get_device_name(name, sizeof(name));
  label = vg_label(root, "蓝牙名称", VG_X(204), LV_TEXT_ALIGN_LEFT,
                   VG_COLOR_MUTED);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, VG_X(18), VG_Y(74));
  label = vg_label(root, name, VG_X(204), LV_TEXT_ALIGN_LEFT, VG_COLOR_TEXT);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, VG_X(18), VG_Y(108));

  label = vg_label(root, "本机 MAC 地址", VG_X(204), LV_TEXT_ALIGN_LEFT,
                   VG_COLOR_MUTED);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, VG_X(18), VG_Y(150));
  label = vg_label(root, addr, VG_X(204), LV_TEXT_ALIGN_LEFT, VG_COLOR_TEXT);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, VG_X(18), VG_Y(184));
  g_vg.bluetooth_addr_label = label;

  lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
}

static void vg_update_bluetooth_page(bool force)
{
  char addr[40];
  uint64_t now;

  if (g_vg.bluetooth_addr_label == NULL)
    {
      return;
    }

  now = vg_uptime_ms();
  if (!force && now - g_vg.bluetooth_last_refresh_ms < 1000)
    {
      return;
    }

  g_vg.bluetooth_last_refresh_ms = now;
  vg_ble_get_local_address(addr, sizeof(addr));
  if (strcmp(addr, "initializing") == 0)
    {
      strlcpy(addr, "蓝牙初始化中", sizeof(addr));
    }
  else if (strcmp(addr, "pending") == 0)
    {
      strlcpy(addr, "地址未就绪", sizeof(addr));
    }

  if (strcmp(lv_label_get_text(g_vg.bluetooth_addr_label), addr) != 0)
    {
      lv_label_set_text(g_vg.bluetooth_addr_label, addr);
    }
}

static void vg_render_bluetooth(void)
{
  if (g_vg.home_root == NULL || g_vg.bluetooth_root == NULL)
    {
      vg_render_home();
    }

  if (g_vg.home_root == NULL || g_vg.bluetooth_root == NULL)
    {
      return;
    }

  g_vg.current_page = VG_PAGE_BLUETOOTH;
  lv_obj_add_flag(g_vg.home_root, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_vg.bluetooth_root, LV_OBJ_FLAG_HIDDEN);
  vg_update_bluetooth_page(true);
}


static void __attribute__((unused)) vg_render_watchface_picker(void)
{
  lv_obj_t *scr = vg_screen_reset();
  lv_obj_t *root;
  lv_obj_t *img;
  lv_obj_t *label;
  lv_obj_t *card;

  g_vg.current_page = VG_PAGE_WATCHFACE_PICKER;
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);
  root = vg_fixed_root(scr);

  label = vg_label(root, "选择表盘", 350, LV_TEXT_ALIGN_CENTER,
                   VG_COLOR_TEXT);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 28);

  card = vg_action_button(root, "", 18, 82, 166, 250, VG_COLOR_CARD_ALT,
                          VG_ACTION_DIAL_RAINBOW);
  img = lv_image_create(card);
  lv_image_set_src(img, &velaguard_img_thumb_rainbow_rain);
  lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 14);

  label = vg_label(card, "流星", 154, LV_TEXT_ALIGN_CENTER,
                   VG_COLOR_TEXT);
  lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -16);

  card = vg_action_button(root, "", 206, 82, 166, 250, VG_COLOR_CARD_ALT,
                          VG_ACTION_DIAL_SIMPLE);
  img = lv_image_create(card);
  lv_image_set_src(img, &velaguard_img_thumb_touch_future);
  lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 14);

  label = vg_label(card, "小猫", 154, LV_TEXT_ALIGN_CENTER,
                   VG_COLOR_TEXT);
  lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -16);

  vg_action_button(root, "返回", 72, 372, 246, 58, VG_COLOR_CARD_ALT,
                   VG_ACTION_BACK);
}


static void vg_render_current(void)
{
  switch (g_vg.state)
    {
      case VG_STATE_PREALERT:
        vg_render_prealert();
        break;

      case VG_STATE_ALERTING:
        vg_render_alert();
        break;

      case VG_STATE_GUARDING:
      default:
        vg_render_home();
        break;
    }
}

static void vg_update_imu_labels(void)
{
  char line[96];
  uint32_t color = VG_COLOR_OK;

  if (g_vg.imu_status_label == NULL ||
      g_vg.imu_value_label == NULL ||
      g_vg.imu_detail_label == NULL)
    {
      return;
    }

  if (!g_vg.imu_ready)
    {
      snprintf(line, sizeof(line), "IMU err=%d", g_vg.imu_last_error);
      lv_label_set_text(g_vg.imu_status_label, line);
      lv_label_set_text(g_vg.imu_value_label, "mag --mg  gyro --dps");
      lv_label_set_text(g_vg.imu_detail_label,
                        CONFIG_CONTEST2026_148_VELAGUARD_IMU_DEVPATH);
      lv_obj_set_style_text_color(g_vg.imu_status_label,
                                  lv_color_hex(VG_COLOR_WARN),
                                  LV_PART_MAIN);
      return;
    }

  if (g_vg.fall_detector.state != 0)
    {
      color = VG_COLOR_WARN;
      lv_label_set_text(g_vg.imu_status_label, "疑似跌倒");
    }
  else
    {
      lv_label_set_text(g_vg.imu_status_label, "IMU 守护中");
    }

  lv_obj_set_style_text_color(g_vg.imu_status_label, lv_color_hex(color),
                              LV_PART_MAIN);

  snprintf(line, sizeof(line), "mag %dmg  gyro %ddps",
           g_vg.imu_mag_mg, g_vg.imu_gyro_dps);
  lv_label_set_text(g_vg.imu_value_label, line);

  snprintf(line, sizeof(line), "peak %d/%d  姿%d  静%dms",
           g_vg.fall_detector.peak_mg,
           g_vg.fall_detector.peak_gyro_dps,
           g_vg.fall_detector.posture_delta_deg,
           g_vg.fall_detector.still_ms);
  lv_label_set_text(g_vg.imu_detail_label, line);
}

static void vg_imu_sample_to_fall(const struct vg_imu_sample_s *imu,
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

static void vg_imu_ui_init(void)
{
  int ret;

  g_vg.imu.fd = -1;
  vg_fall_init(&g_vg.fall_detector);

  ret = vg_imu_open_guarded(&g_vg.imu,
                            CONFIG_CONTEST2026_148_VELAGUARD_IMU_DEVPATH);
  if (ret < 0)
    {
      g_vg.imu_ready = false;
      g_vg.imu_last_error = ret;
      printf("VelaGuard IMU UI: open/probe failed: %d\n", ret);
      vg_update_imu_labels();
      return;
    }

  g_vg.imu_ready = true;
  g_vg.imu_last_error = 0;
  printf("VelaGuard IMU UI: addr=0x%02x whoami=0x%02x\n",
         g_vg.imu.addr, g_vg.imu.whoami);
  vg_update_imu_labels();
}

static void vg_imu_timer_cb(lv_timer_t *timer)
{
  struct vg_imu_sample_s imu_sample;
  struct vg_fall_sample_s fall_sample;
  struct vg_fall_result_s fall_result;
  int ret;

  UNUSED(timer);

  if (!g_vg.imu_ready)
    {
      return;
    }

  if (g_vg.state != VG_STATE_GUARDING)
    {
      return;
    }

  /* IMU now uses the kernel LSM6DSL driver on its dedicated I2C3 bus, so
   * this read no longer needs any pinmux switching or shared-bus locking.
   */

  ret = vg_imu_read_guarded(&g_vg.imu, &imu_sample);
  if (ret < 0)
    {
      g_vg.imu_ready = false;
      g_vg.imu_last_error = ret;
      printf("VelaGuard IMU UI: read failed: %d\n", ret);
      vg_update_imu_labels();
      return;
    }

  vg_imu_sample_to_fall(&imu_sample, &fall_sample);
  g_vg.imu_mag_mg = vg_fall_accel_mag_mg(&fall_sample);
  g_vg.imu_gyro_dps = vg_fall_gyro_sum_dps(&fall_sample);

  if (vg_fall_process(&g_vg.fall_detector, &fall_sample, &fall_result))
    {
      printf("VelaGuard IMU UI: fall detected %s\n", fall_result.reason);
      vg_trigger_fall_result(&fall_result);
      return;
    }

  vg_update_imu_labels();
}

#ifdef CONFIG_INPUT_BUTTONS
static void vg_buttons_init(void)
{
  g_vg.button_fd = open(CONFIG_CONTEST2026_148_VELAGUARD_BUTTON_DEVPATH,
                        O_RDONLY | O_NONBLOCK);
  if (g_vg.button_fd < 0)
    {
      printf("VelaGuard: button device %s unavailable: %d\n",
             CONFIG_CONTEST2026_148_VELAGUARD_BUTTON_DEVPATH, errno);
    }
  else
    {
      /* Do not treat a level present while the GPIO settles as a press. */
      g_vg.last_buttons = 0;
      g_vg.button_down_ms = 0;
      g_vg.button_arm_release_ms = 0;
      g_vg.button_release_candidate_ms = 0;
      g_vg.button_long_handled = false;
      g_vg.button_armed = false;
    }
}

static void vg_buttons_poll(void)
{
  btn_buttonset_t sample;
  ssize_t nread;
  uint64_t now;

  if (g_vg.button_fd < 0)
    {
      return;
    }

  nread = read(g_vg.button_fd, &sample, sizeof(sample));
  if (nread != sizeof(sample))
    {
      return;
    }

  now = vg_uptime_ms();

  /*
   * The button GPIO can be high while the board is powering up.  Arm the
   * long-press detector only after a stable released sample has been seen.
   */
  if (!g_vg.button_armed)
    {
      if (sample & VG_SOS_BUTTON_BIT)
        {
          g_vg.button_arm_release_ms = 0;
        }
      else if (g_vg.button_arm_release_ms == 0)
        {
          g_vg.button_arm_release_ms = now;
        }
      else if (now - g_vg.button_arm_release_ms >=
               VG_BUTTON_RELEASE_DEBOUNCE_MS)
        {
          g_vg.button_armed = true;
          g_vg.button_arm_release_ms = 0;
          printf("VelaGuard UI: SOS button armed after startup release\n");
        }

      g_vg.last_buttons = sample;
      return;
    }

  if ((sample & VG_SOS_BUTTON_BIT) &&
      g_vg.button_down_ms == 0)
    {
      g_vg.button_down_ms = now;
      g_vg.button_release_candidate_ms = 0;
      g_vg.button_long_handled = false;
      printf("VelaGuard UI: SOS button down\n");
    }

  if ((sample & VG_SOS_BUTTON_BIT) && !g_vg.button_long_handled &&
      g_vg.button_down_ms != 0 &&
      now - g_vg.button_down_ms >= VG_SOS_LONG_MS)
    {
      g_vg.button_long_handled = true;
      g_vg.sos_prompt_visible = false;
      printf("VelaGuard UI: SOS long press threshold reached elapsed=%llu ms\n",
             (unsigned long long)(now - g_vg.button_down_ms));
      if (g_vg.state == VG_STATE_GUARDING)
        {
          vg_trigger_event(VG_EVENT_MANUAL_SOS);
        }
    }

  if (!(sample & VG_SOS_BUTTON_BIT) && g_vg.button_down_ms != 0)
    {
      if (g_vg.button_release_candidate_ms == 0)
        {
          g_vg.button_release_candidate_ms = now;
        }
      else if (now - g_vg.button_release_candidate_ms >=
               VG_BUTTON_RELEASE_DEBOUNCE_MS)
        {
          if (g_vg.button_long_handled &&
              g_vg.state == VG_STATE_PREALERT &&
              g_vg.active.type == VG_EVENT_MANUAL_SOS)
            {
              printf("VelaGuard UI: SOS released during countdown, cancel\n");
              vg_cancel_event();
            }
          else if (!g_vg.button_long_handled && g_vg.sos_prompt_visible)
            {
              g_vg.sos_prompt_visible = false;
              vg_schedule_render(VG_RENDER_HOME);
            }

          g_vg.button_down_ms = 0;
          g_vg.button_release_candidate_ms = 0;
          g_vg.button_long_handled = false;
        }
    }
  else if (sample & VG_SOS_BUTTON_BIT)
    {
      g_vg.button_release_candidate_ms = 0;
    }

  g_vg.last_buttons = sample;
}
#endif

static void vg_tick_cb(lv_timer_t *timer)
{
  uint64_t now;
  uint64_t elapsed;
  uint64_t total_ms;

  UNUSED(timer);

#ifdef CONFIG_INPUT_BUTTONS
  vg_buttons_poll();
#endif

  now = vg_uptime_ms();
  vg_update_watchface_time();
  if (g_vg.current_page == VG_PAGE_BLUETOOTH)
    {
      vg_update_bluetooth_page(false);
    }
  vg_update_alarm_hold();

  if (g_vg.alarm_img != NULL)
    {
      int frame = (now / 120) % VG_ALARM_FRAME_COUNT;

      if (frame != g_vg.alarm_frame)
        {
          g_vg.alarm_frame = frame;
          lv_image_set_src(g_vg.alarm_img,
                           g_alarm_frames[g_vg.alarm_frame]);
        }
    }

  if (g_vg.state != VG_STATE_PREALERT)
    {
      g_vg.tick_accum_ms = 0;
      return;
    }

  if (g_vg.countdown_start_ms == 0)
    {
      g_vg.countdown_start_ms = now;
    }

  elapsed = now - g_vg.countdown_start_ms;
  total_ms = (uint64_t)g_vg.countdown_total * 1000;
  if (total_ms == 0)
    {
      return;
    }

  if (elapsed >= total_ms)
    {
      g_vg.countdown = 0;
      vg_update_countdown_visuals();
      vg_confirm_alert();
      return;
    }

  g_vg.countdown = g_vg.countdown_total - (int)(elapsed / 1000);
  if (g_vg.countdown != g_vg.countdown_last_value)
    {
      g_vg.countdown_last_value = g_vg.countdown;
      vg_update_countdown_visuals();
    }

  if (g_vg.active.type == VG_EVENT_MANUAL_SOS &&
      g_vg.countdown_arc != NULL)
    {
      vg_update_manual_sos_progress(now);
    }
}

static void vg_update_manual_sos_progress(uint64_t now)
{
  uint64_t elapsed;
  uint64_t total_ms;
  int arc_value;

  if (g_vg.countdown_arc == NULL || g_vg.countdown_start_ms == 0 ||
      g_vg.countdown_total <= 0)
    {
      return;
    }

  total_ms = (uint64_t)g_vg.countdown_total * 1000;
  elapsed = now - g_vg.countdown_start_ms;
  if (elapsed >= total_ms)
    {
      arc_value = 0;
    }
  else
    {
      arc_value = (int)(((total_ms - elapsed) * 100) / total_ms);
    }

  lv_arc_set_value(g_vg.countdown_arc, arc_value);
}

static void vg_audio_process(void)
{
  uint64_t now;

  if (!g_vg.audio_ready ||
      !vg_audio_capture_level(&g_vg.audio_level, &g_vg.audio_sequence,
                              &g_vg.audio_keyword))
    {
      return;
    }

  now = vg_uptime_ms();
  g_vg.audio_report_tick += 32;
  if (g_vg.audio_report_tick >= 1000)
    {
      printf("VelaGuard MIC: seq=%lu dc=%ld mean=%lu peak=%u voice=%d\n",
             (unsigned long)g_vg.audio_sequence,
             (long)g_vg.audio_level.dc,
             (unsigned long)g_vg.audio_level.mean_abs,
             g_vg.audio_level.peak,
             g_vg.audio_level.speech_active ? 1 : 0);
      g_vg.audio_report_tick = 0;
    }

  if (g_vg.audio_keyword != VG_AUDIO_KEYWORD_NONE &&
      g_vg.state == VG_STATE_GUARDING &&
      (g_vg.voice_last_trigger_ms == 0 ||
       now - g_vg.voice_last_trigger_ms >= VG_VOICE_REARM_MS))
    {
      g_vg.voice_last_trigger_ms = now;
      printf("VelaGuard KWS: keyword=%s recognized\n",
             g_vg.audio_keyword == VG_AUDIO_KEYWORD_JIUMING ?
             "救命" : "求助");
      vg_trigger_event(VG_EVENT_VOICE);
    }
}

static int vg_wait_for_device(const char *path, unsigned int timeout_ms)
{
  unsigned int waited_ms = 0;

  while (access(path, F_OK) < 0)
    {
      if (waited_ms >= timeout_ms)
        {
          printf("VelaGuard: timeout waiting for %s\n", path);
          return -ETIMEDOUT;
        }

      usleep(VG_DEVICE_WAIT_STEP_MS * 1000);
      waited_ms += VG_DEVICE_WAIT_STEP_MS;
    }

  printf("VelaGuard: device ready %s after %u ms\n", path, waited_ms);
  return 0;
}

static void vg_audio_start_once(void)
{
#ifdef CONFIG_CONTEST2026_148_MIC_CAPTURE
  if (g_vg.audio_start_attempted)
    {
      return;
    }

  g_vg.audio_start_attempted = true;
  if (vg_audio_capture_start() == 0)
    {
      g_vg.audio_ready = true;
      printf("VelaGuard audio: capture started before BLE bring-up\n");
    }
  else
    {
      printf("VelaGuard audio: microphone capture unavailable\n");
    }
#else
  printf("VelaGuard audio: microphone capture disabled\n");
#endif
}

static void vg_audio_feedback_start(enum vg_feedback_type_e type)
{
#ifdef CONFIG_CONTEST2026_148_AUDIO_FEEDBACK
  if (vg_audio_feedback_active() || vg_audio_feedback_trigger(type) < 0)
    {
      return;
    }

  /* SF32LB52 uses one codec/DMA complex for ADC capture and DAC playback.
   * Do not leave capture's message queue and DMA in flight while opening the
   * playback stream.  Feedback is brief; capture resumes on completion. */
  if (g_vg.audio_ready)
    {
      printf("VelaGuard audio: pause capture for feedback\n");
      vg_audio_capture_stop();
      g_vg.audio_ready = false;
      g_vg.audio_resume_after_feedback = true;
    }
#else
  (void)type;
#endif
}

static void vg_audio_resume_after_feedback(void)
{
#if defined(CONFIG_CONTEST2026_148_AUDIO_FEEDBACK) && \
    defined(CONFIG_CONTEST2026_148_MIC_CAPTURE)
  if (!g_vg.audio_resume_after_feedback || vg_audio_feedback_active())
    {
      return;
    }

  g_vg.audio_resume_after_feedback = false;
  if (vg_audio_capture_start() == 0)
    {
      g_vg.audio_ready = true;
      printf("VelaGuard audio: capture resumed after feedback\n");
    }
  else
    {
      printf("VelaGuard audio: capture resume failed\n");
    }
#endif
}

static void vg_audio_headless_loop(void)
{
  unsigned int report_ms = 0;

  printf("VelaGuard: UI unavailable; audio diagnostics remain active.\n");
  for (; ; )
    {
      vg_ble_service_poll();
      vg_ble_process_time();
      vg_audio_process();

      if (g_vg.audio_ready &&
          vg_audio_capture_level(&g_vg.audio_level, &g_vg.audio_sequence,
                                 NULL))
        {
          report_ms += VG_DEVICE_WAIT_STEP_MS;
          if (report_ms >= 1000)
            {
              printf("VelaGuard MIC: seq=%lu dc=%ld mean=%lu peak=%u voice=%d\n",
                     (unsigned long)g_vg.audio_sequence,
                     (long)g_vg.audio_level.dc,
                     (unsigned long)g_vg.audio_level.mean_abs,
                     g_vg.audio_level.peak,
                     g_vg.audio_level.speech_active ? 1 : 0);
              report_ms = 0;
            }
        }

      usleep(VG_DEVICE_WAIT_STEP_MS * 1000);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;
  bool input_ready = false;
  int warmup;
  int ret;

  (void)argc;
  (void)argv;

  memset(&g_vg, 0, sizeof(g_vg));
  g_vg.state = VG_STATE_GUARDING;
  g_vg.mode = VG_MODE_ELDER;
  g_vg.watchface = 1;
  g_vg.next_id = 1000;
  g_vg.home_last_minute = -1;
  g_vg.home_last_mday = -1;
#ifdef CONFIG_INPUT_BUTTONS
  g_vg.button_fd = -1;
#endif

  if (lv_is_initialized())
    {
      printf("VelaGuard: LVGL already initialized.\n");
      return -1;
    }

#ifdef NEED_BOARDINIT
  boardctl(BOARDIOC_INIT, 0);
#endif

#ifdef CONFIG_LV_USE_NUTTX_LCD
  vg_wait_for_device("/dev/lcd0", VG_DEVICE_WAIT_MS);
#endif

#ifdef CONFIG_INPUT_TOUCHSCREEN
  /* Board bring-up registers FT6146 asynchronously.  lv_nuttx_init() only
   * attempts to open input_path once, so starting LVGL before /dev/input0
   * exists leaves the UI permanently without an input device.
   */

  input_ready = vg_wait_for_device(
    CONFIG_CONTEST2026_148_VELAGUARD_INPUT_DEVPATH,
    VG_DEVICE_WAIT_MS) == 0;
#endif

  /* Complete the audio DMA setup before Bluetooth begins ATT/GATT work.
   * The audio capture path is non-blocking after this point. */
  vg_audio_start_once();

  /* Keep Framework/H4 ownership and initialization ahead of LVGL.  Retrying
   * bluetooth_create_instance() from the UI loop can create a second client
   * after a partial bring-up, so a failed startup is reported and left intact
   * for diagnosis until the next system boot. */
  printf("VelaGuard BLE: service init attempt\n");
  ret = vg_ble_init();
  printf("VelaGuard BLE: init ret=%d\n", ret);
  if (ret < 0)
    {
      printf("VelaGuard BLE: initialization failed; retry is disabled until reboot\n");
    }

#ifdef CONFIG_CONTEST2026_148_VELAGUARD_HEADLESS
  printf("VelaGuard: headless BLE isolation mode\n");
  vg_audio_headless_loop();
  return 0;
#endif

  lv_init();
  lv_nuttx_dsc_init(&info);

#ifdef CONFIG_LV_USE_NUTTX_LCD
  info.fb_path = "/dev/lcd0";
#endif

#ifdef CONFIG_INPUT_TOUCHSCREEN
  if (input_ready)
    {
      info.input_path = CONFIG_CONTEST2026_148_VELAGUARD_INPUT_DEVPATH;
    }
#endif

  lv_nuttx_init(&info, &result);
  if (result.disp == NULL)
    {
      printf("VelaGuard: LVGL NuttX display initialization failed.\n");
      lv_deinit();
      vg_audio_headless_loop();
      return 1;
    }

  if (result.indev != NULL)
    {
      lv_timer_t *read_timer = lv_indev_get_read_timer(result.indev);

      if (read_timer != NULL)
        {
          lv_timer_set_period(read_timer,
                              CONFIG_CONTEST2026_148_VELAGUARD_INPUT_POLL_MS);
        }
    }
  else
    {
      printf("VelaGuard: LVGL touch input initialization failed\n");
    }

#ifdef CONFIG_INPUT_BUTTONS
  vg_buttons_init();
#endif

  printf("VelaGuard: started. JSON events are printed as VELAGUARD_EVENT.\n");
  fflush(stdout);

  vg_render_home();
  vg_imu_ui_init();
  g_vg.tick_timer = lv_timer_create(vg_tick_cb, VG_TICK_PERIOD_MS, NULL);
  g_vg.imu_timer = lv_timer_create(vg_imu_timer_cb, VG_IMU_UI_PERIOD_MS,
                                   NULL);
  g_vg.render_timer = lv_timer_create(vg_render_timer_cb, 1, NULL);
  lv_timer_pause(g_vg.render_timer);

  for (warmup = 0; warmup < 3; warmup++)
    {
      lv_timer_handler();
      usleep(20000);
    }

  for (; ; )
    {
      /* Audio capture has already been initialized before BLE; the loop only
       * polls its non-blocking message queue after advancing BLE state. */
      vg_ble_service_poll();
      vg_ble_process_time();

      vg_audio_process();
#ifdef CONFIG_CONTEST2026_148_AUDIO_FEEDBACK
      vg_audio_feedback_process();
      vg_audio_resume_after_feedback();
#endif

      uint32_t idle = lv_timer_handler();

      idle = idle ? idle : 1;
      if (idle > CONFIG_CONTEST2026_148_VELAGUARD_LOOP_SLEEP_MAX_MS)
        {
          idle = CONFIG_CONTEST2026_148_VELAGUARD_LOOP_SLEEP_MAX_MS;
        }

      usleep(idle * 1000);
    }

  return 0;
}
