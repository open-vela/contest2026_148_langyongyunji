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

#include <sys/boardctl.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
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

#include <lvgl/lvgl.h>

#include "velaguard_fall.h"
#include "velaguard_imu.h"

LV_FONT_DECLARE(velaguard_font_18);

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
#  define CONFIG_CONTEST2026_148_VELAGUARD_IMU_DEVPATH "/dev/i2c0"
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
#define VG_TICK_PERIOD_MS    250
#define VG_IMU_UI_PERIOD_MS  100

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
  VG_ACTION_MODE,
  VG_ACTION_BACK,
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
  int tick_accum_ms;
  int imu_mag_mg;
  int imu_gyro_dps;
  int imu_last_error;
  uint32_t next_id;
  bool has_fall_result;
  bool imu_ready;
  lv_obj_t *countdown_label;
  lv_obj_t *detail_label;
  lv_obj_t *imu_status_label;
  lv_obj_t *imu_value_label;
  lv_obj_t *imu_detail_label;
  lv_timer_t *tick_timer;
  lv_timer_t *imu_timer;
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
static void vg_render_current(void);
static void vg_set_font(lv_obj_t *obj);
static void vg_trigger_fall_result(const struct vg_fall_result_s *result);
static void vg_update_imu_labels(void);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct vg_app_s g_vg;

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
  if (g_vg.state == VG_STATE_ALERTING)
    {
      return;
    }

  g_vg.state = VG_STATE_ALERTING;
  g_vg.active.timestamp_ms = vg_uptime_ms();
  vg_make_summary(&g_vg.active, "alert");
  vg_history_push(&g_vg.active);
  vg_emit_json(&g_vg.active);
  vg_render_alert();
}

static void vg_trigger_event(enum vg_event_type_e type)
{
  int risk = 3;
  int confidence = 88;

  g_vg.has_fall_result = false;

  if (type == VG_EVENT_MANUAL_SOS)
    {
      vg_prepare_event(type, 4, 100, "alert");
      g_vg.state = VG_STATE_ALERTING;
      vg_history_push(&g_vg.active);
      vg_emit_json(&g_vg.active);
      vg_render_alert();
      return;
    }

  if (type == VG_EVENT_SOUND)
    {
      confidence = 76;
    }
  else if (type == VG_EVENT_VOICE)
    {
      confidence = 92;
    }

  vg_prepare_event(type, risk, confidence, "suspected");
  g_vg.state = VG_STATE_PREALERT;
  g_vg.countdown = CONFIG_CONTEST2026_148_VELAGUARD_COUNTDOWN;
  g_vg.tick_accum_ms = 0;
  vg_emit_json(&g_vg.active);
  vg_render_prealert();
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

static void vg_show_fall_popup(const struct vg_fall_result_s *result)
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

  vg_prepare_event(VG_EVENT_FALL, result->risk, result->confidence,
                   "suspected");
  g_vg.state = VG_STATE_PREALERT;
  g_vg.countdown = CONFIG_CONTEST2026_148_VELAGUARD_COUNTDOWN;
  g_vg.tick_accum_ms = 0;
  vg_emit_json(&g_vg.active);
  vg_render_prealert();
  vg_show_fall_popup(result);
}

static void vg_cancel_event(void)
{
  if (g_vg.state != VG_STATE_PREALERT)
    {
      vg_render_home();
      return;
    }

  g_vg.active.timestamp_ms = vg_uptime_ms();
  g_vg.active.risk = 0;
  vg_make_summary(&g_vg.active, "cancelled");
  vg_history_push(&g_vg.active);
  vg_emit_json(&g_vg.active);

  g_vg.state = VG_STATE_GUARDING;
  g_vg.tick_accum_ms = 0;
  vg_render_home();
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
  vg_render_home();
}

static lv_obj_t *vg_screen_reset(void)
{
  lv_obj_t *scr = lv_screen_active();

  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(VG_COLOR_BG), LV_PART_MAIN);
  lv_obj_set_style_text_color(scr, lv_color_hex(VG_COLOR_TEXT),
                              LV_PART_MAIN);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  g_vg.countdown_label = NULL;
  g_vg.detail_label = NULL;
  g_vg.imu_status_label = NULL;
  g_vg.imu_value_label = NULL;
  g_vg.imu_detail_label = NULL;

  return scr;
}

static void vg_set_font(lv_obj_t *obj)
{
  lv_obj_set_style_text_font(obj, &velaguard_font_18, LV_PART_MAIN);
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

static lv_obj_t *vg_action_button(lv_obj_t *parent, const char *text,
                                  int32_t x, int32_t y, int32_t w,
                                  int32_t h, uint32_t color,
                                  enum vg_action_e action);

static void vg_action_cb(lv_event_t *event)
{
  enum vg_action_e action;

  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
    {
      return;
    }

  action = (enum vg_action_e)(uintptr_t)lv_event_get_user_data(event);

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
        vg_render_history();
        break;

      case VG_ACTION_SETTINGS:
        vg_render_settings();
        break;

      case VG_ACTION_MODE:
        g_vg.mode = (g_vg.mode + 1) % 4;
        vg_render_settings();
        break;

      case VG_ACTION_BACK:
      default:
        vg_render_current();
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
  lv_obj_set_style_text_color(label, lv_color_hex(VG_COLOR_TEXT),
                              LV_PART_MAIN);
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

static void vg_render_home(void)
{
  lv_obj_t *scr = vg_screen_reset();
  lv_obj_t *label;
  char line[96];
  int32_t w = lv_display_get_horizontal_resolution(NULL);
  int32_t bw = (w - 36) / 2;

  vg_render_header(scr, "守护", VG_COLOR_OK);

  label = vg_label(scr, "安全守护运行中", w - 24,
                   LV_TEXT_ALIGN_CENTER, VG_COLOR_TEXT);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 70);

  snprintf(line, sizeof(line), "模式：%s  连接：串口上报",
           vg_mode_name(g_vg.mode));
  label = vg_label(scr, line, w - 24, LV_TEXT_ALIGN_CENTER, VG_COLOR_MUTED);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 102);

  g_vg.imu_status_label = vg_label(scr, "IMU 启动中", w - 24,
                                   LV_TEXT_ALIGN_CENTER, VG_COLOR_MUTED);
  lv_obj_align(g_vg.imu_status_label, LV_ALIGN_TOP_MID, 0, 150);

  g_vg.imu_value_label = vg_label(scr, "mag --mg  gyro --dps", w - 24,
                                  LV_TEXT_ALIGN_CENTER, VG_COLOR_MUTED);
  lv_obj_align(g_vg.imu_value_label, LV_ALIGN_TOP_MID, 0, 184);

  g_vg.imu_detail_label = vg_label(scr, "peak --/--  姿态 --  静止 --ms",
                                   w - 24, LV_TEXT_ALIGN_CENTER,
                                   VG_COLOR_MUTED);
  lv_obj_align(g_vg.imu_detail_label, LV_ALIGN_TOP_MID, 0, 216);
  vg_update_imu_labels();

  vg_action_button(scr, "SOS求助", 12, 300, w - 24, 72, VG_COLOR_ALERT,
                   VG_ACTION_SOS);
  vg_action_button(scr, "事件记录", 12, 392, bw, 40, VG_COLOR_CARD_ALT,
                   VG_ACTION_HISTORY);
  vg_action_button(scr, "设置", 24 + bw, 392, bw, 40, VG_COLOR_CARD_ALT,
                   VG_ACTION_SETTINGS);
}

static void vg_render_prealert(void)
{
  lv_obj_t *scr = vg_screen_reset();
  lv_obj_t *label;
  char line[32];
  int32_t w = lv_display_get_horizontal_resolution(NULL);
  int32_t bw = (w - 36) / 2;

  vg_render_header(scr, "确认", VG_COLOR_WARN);

  label = vg_label(scr, vg_event_title(g_vg.active.type), w - 24,
                   LV_TEXT_ALIGN_CENTER, VG_COLOR_TEXT);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 70);

  snprintf(line, sizeof(line), "%d", g_vg.countdown);
  g_vg.countdown_label = vg_label(scr, line, w - 24, LV_TEXT_ALIGN_CENTER,
                                  VG_COLOR_WARN);
  lv_obj_align(g_vg.countdown_label, LV_ALIGN_TOP_MID, 0, 116);

  label = vg_label(scr, "秒后自动告警", w - 24,
                   LV_TEXT_ALIGN_CENTER, VG_COLOR_MUTED);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 160);

  g_vg.detail_label = vg_label(scr, g_vg.active.summary, w - 32,
                               LV_TEXT_ALIGN_CENTER, VG_COLOR_MUTED);
  lv_obj_align(g_vg.detail_label, LV_ALIGN_TOP_MID, 0, 206);

  vg_action_button(scr, "取消", 12, 326, bw, 64, VG_COLOR_CARD_ALT,
                   VG_ACTION_CANCEL);
  vg_action_button(scr, "立即告警", 24 + bw, 326, bw, 64, VG_COLOR_ALERT,
                   VG_ACTION_CONFIRM);
  vg_action_button(scr, "返回守护", 12, 406, w - 24, 34, VG_COLOR_CARD_ALT,
                   VG_ACTION_CANCEL);
}

static void vg_render_alert(void)
{
  lv_obj_t *scr = vg_screen_reset();
  lv_obj_t *label;
  char line[64];
  int32_t w = lv_display_get_horizontal_resolution(NULL);

  vg_render_header(scr, "告警", VG_COLOR_ALERT);

  label = vg_label(scr, "SOS 告警中", w - 24, LV_TEXT_ALIGN_CENTER,
                   VG_COLOR_ALERT);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 72);

  label = vg_label(scr, vg_event_title(g_vg.active.type), w - 24,
                   LV_TEXT_ALIGN_CENTER, VG_COLOR_TEXT);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 114);

  snprintf(line, sizeof(line), "风险 %d  置信度 %d%%",
           g_vg.active.risk, g_vg.active.confidence);
  label = vg_label(scr, line, w - 24, LV_TEXT_ALIGN_CENTER, VG_COLOR_WARN);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 150);

  label = vg_label(scr, g_vg.active.summary, w - 32, LV_TEXT_ALIGN_CENTER,
                   VG_COLOR_MUTED);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 196);

  label = vg_label(scr, "本地告警：屏幕 + 串口上报", w - 24,
                   LV_TEXT_ALIGN_CENTER, VG_COLOR_MUTED);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 284);

  vg_action_button(scr, "解除", 12, 334, w - 24, 62, VG_COLOR_OK,
                   VG_ACTION_RESOLVE);
  vg_action_button(scr, "事件记录", 12, 410, w - 24, 34, VG_COLOR_CARD_ALT,
                   VG_ACTION_HISTORY);
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
      lv_label_set_text(g_vg.imu_detail_label, "/dev/i2c0 0x6a");
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
}

static void vg_buttons_poll(void)
{
  btn_buttonset_t sample;
  ssize_t nread;

  if (g_vg.button_fd < 0)
    {
      return;
    }

  nread = read(g_vg.button_fd, &sample, sizeof(sample));
  if (nread != sizeof(sample))
    {
      return;
    }

  if ((sample & 0x01) && !(g_vg.last_buttons & 0x01))
    {
      vg_trigger_event(VG_EVENT_FALL);
    }
  else if ((sample & 0x02) && !(g_vg.last_buttons & 0x02))
    {
      vg_trigger_event(VG_EVENT_MANUAL_SOS);
    }

  g_vg.last_buttons = sample;
}
#endif

static void vg_tick_cb(lv_timer_t *timer)
{
  char line[16];

  UNUSED(timer);

#ifdef CONFIG_INPUT_BUTTONS
  vg_buttons_poll();
#endif

  if (g_vg.state != VG_STATE_PREALERT)
    {
      g_vg.tick_accum_ms = 0;
      return;
    }

  g_vg.tick_accum_ms += VG_TICK_PERIOD_MS;
  if (g_vg.tick_accum_ms < 1000)
    {
      return;
    }

  g_vg.tick_accum_ms -= 1000;

  if (g_vg.countdown > 0)
    {
      g_vg.countdown--;
    }

  if (g_vg.countdown_label != NULL)
    {
      snprintf(line, sizeof(line), "%d", g_vg.countdown);
      lv_label_set_text(g_vg.countdown_label, line);
    }

  if (g_vg.countdown <= 0)
    {
      vg_confirm_alert();
    }
}

static enum vg_event_type_e vg_parse_demo_type(const char *arg)
{
  if (strcmp(arg, "fall") == 0)
    {
      return VG_EVENT_FALL;
    }

  if (strcmp(arg, "sound") == 0)
    {
      return VG_EVENT_SOUND;
    }

  if (strcmp(arg, "voice") == 0)
    {
      return VG_EVENT_VOICE;
    }

  if (strcmp(arg, "sos") == 0)
    {
      return VG_EVENT_MANUAL_SOS;
    }

  return VG_EVENT_FALL;
}

static void vg_usage(void)
{
  printf("用法: velaguard [--demo fall|sound|voice|sos] [--imu-scan] [--imu-test [n]] [--fall-watch [seconds]]\n");
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;
  bool has_initial_event = false;
  bool imu_scan = false;
  bool imu_test = false;
  bool fall_watch = false;
  enum vg_event_type_e initial_event = VG_EVENT_FALL;
  int imu_samples = 30;
  int fall_watch_seconds = 30;
  int i;

  for (i = 1; i < argc; i++)
    {
      if (strcmp(argv[i], "--help") == 0)
        {
          vg_usage();
          return 0;
        }
      else if (strcmp(argv[i], "--demo") == 0 && i + 1 < argc)
        {
          initial_event = vg_parse_demo_type(argv[++i]);
          has_initial_event = true;
        }
      else if (strcmp(argv[i], "--imu-test") == 0)
        {
          imu_test = true;
          if (i + 1 < argc && argv[i + 1][0] >= '0' &&
              argv[i + 1][0] <= '9')
            {
              imu_samples = atoi(argv[++i]);
              if (imu_samples < 1)
                {
                  imu_samples = 1;
                }
            }
        }
      else if (strcmp(argv[i], "--imu-scan") == 0)
        {
          imu_scan = true;
        }
      else if (strcmp(argv[i], "--fall-watch") == 0)
        {
          fall_watch = true;
          if (i + 1 < argc && argv[i + 1][0] >= '0' &&
              argv[i + 1][0] <= '9')
            {
              fall_watch_seconds = atoi(argv[++i]);
              if (fall_watch_seconds < 1)
                {
                  fall_watch_seconds = 30;
                }
            }
        }
      else
        {
          vg_usage();
          return 1;
        }
    }

  if (imu_scan)
    {
      return vg_imu_scan() < 0 ? 1 : 0;
    }

  if (imu_test)
    {
      return vg_imu_selftest(CONFIG_CONTEST2026_148_VELAGUARD_IMU_DEVPATH,
                             imu_samples) < 0 ? 1 : 0;
    }

  if (fall_watch)
    {
      return vg_imu_fall_watch(CONFIG_CONTEST2026_148_VELAGUARD_IMU_DEVPATH,
                               fall_watch_seconds) < 0 ? 1 : 0;
    }

  memset(&g_vg, 0, sizeof(g_vg));
  g_vg.state = VG_STATE_GUARDING;
  g_vg.mode = VG_MODE_ELDER;
  g_vg.next_id = 1000;
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

  lv_init();
  lv_nuttx_dsc_init(&info);

#ifdef CONFIG_LV_USE_NUTTX_LCD
  info.fb_path = "/dev/lcd0";
#endif

#ifdef CONFIG_INPUT_TOUCHSCREEN
  info.input_path = CONFIG_CONTEST2026_148_VELAGUARD_INPUT_DEVPATH;
#endif

  lv_nuttx_init(&info, &result);
  if (result.disp == NULL)
    {
      printf("VelaGuard: LVGL NuttX display initialization failed.\n");
      lv_deinit();
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

  if (has_initial_event)
    {
      vg_trigger_event(initial_event);
    }

  for (; ; )
    {
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
