# Contest 2026 team 148 - VelaGuard
#
# NUTTX_BOARD_DIR is overwritten by the custom board dir (Huangshan Pi BSP)
# when CONFIG_ARCH_BOARD_CUSTOM=y.  Contest-specific board sources live in
# vendor/openvela/boards/contest2026_148_board/src/ and must be referenced
# explicitly.

set(CONTEST2026_148_BOARD_DIR
    ${NUTTX_DIR}/../vendor/openvela/boards/contest2026_148_board)

if(CONFIG_LVX_USE_DEMO_CONTEST2026_148_VELAGUARD)
  nuttx_add_application(
    NAME
    velaguard
    PRIORITY
    ${CONFIG_CONTEST2026_148_VELAGUARD_PRIORITY}
    STACKSIZE
    ${CONFIG_CONTEST2026_148_VELAGUARD_STACKSIZE}
    MODULE
    ${CONFIG_LVX_USE_DEMO_CONTEST2026_148_VELAGUARD}
    DEPENDS
    lvgl
    DEFINITIONS
    SOC_BF0_HCPU
    SF32LB52X
    USE_HAL_DRIVER
    INCLUDE_DIRECTORIES
    ${NUTTX_BOARD_DIR}/include
    ${NUTTX_DIR}/../vendor/sifli/chips/drivers/Include
    ${NUTTX_DIR}/../vendor/sifli/chips/drivers/cmsis/Include
    ${NUTTX_DIR}/../vendor/sifli/chips/drivers/cmsis/sf32lb52x
    ${NUTTX_DIR}/../vendor/sifli/chips/external/CMSIS/Include
    ${NUTTX_DIR}/../vendor/sifli/chips/sf32lb52
    ${NUTTX_DIR}/../vendor/sifli/chips/sf32lb52/include
    ${NUTTX_DIR}/../vendor/sifli/chips/boards/include
    ${NUTTX_DIR}/../apps/external/zblue/zblue/port/include
    ${NUTTX_DIR}/../apps/external/zblue/zblue/port/kernel/include
    ${CMAKE_BINARY_DIR}/apps/external/zblue/include
    ${CMAKE_BINARY_DIR}/apps/external/zblue/include/zephyr/sys
    ${CMAKE_BINARY_DIR}/apps/external/zblue/include/zephyr/kernel
    ${CMAKE_BINARY_DIR}/apps/external/zblue/include/zephyr/arch
    ${CMAKE_BINARY_DIR}/apps/external/zblue/include/zephyr/linker
    ${CMAKE_BINARY_DIR}/apps/external/zblue/include/zephyr/devicetree
    ${CMAKE_BINARY_DIR}/apps/external/zblue/include/zephyr/debug
    ${CMAKE_BINARY_DIR}/apps/external/zblue/include/zephyr/fs
    ${CMAKE_BINARY_DIR}/apps/external/zblue/include/zephyr/shell
    ${CMAKE_BINARY_DIR}/apps/external/zblue/include/zephyr/tracing
    ${CMAKE_BINARY_DIR}/apps/external/zblue/include/zephyr/app_memory
    ${NUTTX_DIR}/../apps/external/zblue/zblue/include
    ${NUTTX_DIR}/arch/arm/src/common
    ${NUTTX_DIR}/arch/arm/src/armv8-m
    ${NUTTX_DIR}/arch/arm/src/arm_m
    ${NUTTX_DIR}/sched
    core
    algorithm
    audio
    sensors
    comm
    ui
    SRCS
    core/velaguard_main.c
    core/velaguard_boot.c
    algorithm/velaguard_fall.c
    audio/velaguard_audio.c
    audio/velaguard_audio_feedback.c
    audio/velaguard_wav_parser.c
    sensors/velaguard_imu.c
    comm/velaguard_ble.c
    ${NUTTX_DIR}/../apps/external/zblue/zblue/port/drivers/bluetooth/hci/h4.c
    ui/velaguard_font_18.c)

  if(CONFIG_CONTEST2026_148_AUDIO_TEST)
    nuttx_add_application(
      NAME
      audio_test
      PRIORITY
      100
      STACKSIZE
      16384
      MODULE
      ${CONFIG_CONTEST2026_148_AUDIO_TEST}
      SRCS
      audio/audio_test.c)
  endif()
endif()
