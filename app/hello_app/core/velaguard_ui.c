/****************************************************************************
 * VelaGuard LVGL platform initialization and processing.
 ****************************************************************************/

#include "velaguard_ui.h"

#include <nuttx/config.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <lvgl/lvgl.h>

static const char *g_lcd_path;
static const char *g_input_path;
static bool g_input_ready;

static int vg_ui_wait_for_device(const char *path, unsigned int timeout_ms,
                                 unsigned int wait_step_ms)
{
  unsigned int waited_ms = 0;

  if (path == NULL)
    {
      return -EINVAL;
    }

  while (access(path, F_OK) < 0)
    {
      if (waited_ms >= timeout_ms)
        {
          printf("VelaGuard UI: timeout waiting for %s\n", path);
          return -ETIMEDOUT;
        }

      usleep(wait_step_ms * 1000);
      waited_ms += wait_step_ms;
    }

  printf("VelaGuard: device ready %s after %u ms\n", path, waited_ms);
  return 0;
}

int vg_ui_prepare(const char *lcd_path, const char *input_path,
                  unsigned int timeout_ms, unsigned int wait_step_ms)
{
  g_lcd_path = lcd_path;
  g_input_path = input_path;
  g_input_ready = false;

#ifdef CONFIG_LV_USE_NUTTX_LCD
  (void)vg_ui_wait_for_device(g_lcd_path, timeout_ms, wait_step_ms);
#endif

#ifdef CONFIG_INPUT_TOUCHSCREEN
  /* The FT6146 is registered asynchronously.  lv_nuttx_init() only tries
   * input_path once, so wait before the application starts its services. */
  g_input_ready = vg_ui_wait_for_device(g_input_path, timeout_ms,
                                         wait_step_ms) == 0;
#endif

  return 0;
}

int vg_ui_init(unsigned int input_poll_ms)
{
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;

  if (lv_is_initialized())
    {
      printf("VelaGuard: LVGL already initialized.\n");
      return -EALREADY;
    }

  lv_init();
  lv_nuttx_dsc_init(&info);

#ifdef CONFIG_LV_USE_NUTTX_LCD
  info.fb_path = g_lcd_path;
#endif

#ifdef CONFIG_INPUT_TOUCHSCREEN
  if (g_input_ready)
    {
      info.input_path = g_input_path;
    }
#endif

  lv_nuttx_init(&info, &result);
  if (result.disp == NULL)
    {
      printf("VelaGuard: LVGL NuttX display initialization failed.\n");
      lv_deinit();
      return -ENODEV;
    }

  if (result.indev != NULL)
    {
      lv_timer_t *read_timer = lv_indev_get_read_timer(result.indev);

      if (read_timer != NULL)
        {
          lv_timer_set_period(read_timer, input_poll_ms);
        }
    }
  else
    {
      printf("VelaGuard: LVGL touch input initialization failed\n");
    }

  return 0;
}

uint32_t vg_ui_process(void)
{
  return lv_timer_handler();
}
