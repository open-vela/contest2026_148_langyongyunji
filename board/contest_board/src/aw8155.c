/****************************************************************************
 * vendor/openvela/boards/contest2026_148_board/src/aw8155.c
 *
 * AW8155 Audio PA on/off control via GPIO PA42.
 * Reference: SiFli SDK customer/peripherals/pa/AW8155/
 *
 * Control protocol:
 *   - AW8155 is controlled by a single GPIO pin (PA42)
 *   - ON:  Send N pulse sequence (N = mode, 1-4)
 *          Each pulse: GPIO_HIGH(5μs) → GPIO_LOW(5μs)
 *          Then wait 100ms for PA to stabilize
 *   - OFF: Pull GPIO LOW
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/arch.h>
#include <stdbool.h>

#include "bf0_hal.h"
#include "aw8155.h"

/* PA42 is on hwp_gpio1 (Port A), pin index 42.
 * Already configured as GPIO_A42 in bsp_pinmux.c.
 */

#define AW8155_GPIO_PORT    hwp_gpio1
#define AW8155_GPIO_PIN     42
#define AW8155_PULSE_US     5       /* 5us per pulse half-cycle */
#define AW8155_STABLE_MS    100     /* 100ms PA stabilization wait */

static bool g_initialized;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void aw8155_gpio_init(void)
{
  GPIO_InitTypeDef init;

  if (g_initialized)
    {
      return;
    }

  init.Mode  = GPIO_MODE_OUTPUT;
  init.Pin   = AW8155_GPIO_PIN;
  init.Pull  = GPIO_NOPULL;
  HAL_GPIO_Init(AW8155_GPIO_PORT, &init);

  /* Start with PA off */
  HAL_GPIO_WritePin(AW8155_GPIO_PORT, AW8155_GPIO_PIN, GPIO_PIN_RESET);

  g_initialized = true;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void aw8155_pa_on(int mode)
{
    int i;

    if (mode < 1) mode = 1;
    if (mode > 4) mode = 4;

    aw8155_gpio_init();

    /* Send N pulse sequence to select work mode */
    for (i = 0; i < mode; i++)
    {
        HAL_GPIO_WritePin(AW8155_GPIO_PORT, AW8155_GPIO_PIN, GPIO_PIN_SET);
        up_udelay(AW8155_PULSE_US);

        HAL_GPIO_WritePin(AW8155_GPIO_PORT, AW8155_GPIO_PIN, GPIO_PIN_RESET);
        up_udelay(AW8155_PULSE_US);
    }

    /* Wait for PA to stabilize before audio output */
    up_udelay(AW8155_STABLE_MS * 1000);
}

void aw8155_pa_off(void)
{
    aw8155_gpio_init();
    HAL_GPIO_WritePin(AW8155_GPIO_PORT, AW8155_GPIO_PIN, GPIO_PIN_RESET);
}
