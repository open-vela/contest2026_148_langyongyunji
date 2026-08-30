/****************************************************************************
 * VelaGuard BLE compatibility shims for framework build variants.
 ****************************************************************************/

#include <nuttx/config.h>

#include "bt_device.h"

void __attribute__((weak)) bt_pm_init(void)
{
}

void __attribute__((weak)) bt_pm_cleanup(void)
{
}

bt_status_t __attribute__((weak))
bt_device_enable_enhanced_mode(bt_instance_t *ins, bt_address_t *addr,
                               bt_enhanced_mode_t mode)
{
  return BT_STATUS_FAIL;
}

bt_status_t __attribute__((weak))
bt_device_disable_enhanced_mode(bt_instance_t *ins, bt_address_t *addr,
                                bt_enhanced_mode_t mode)
{
  return BT_STATUS_FAIL;
}
