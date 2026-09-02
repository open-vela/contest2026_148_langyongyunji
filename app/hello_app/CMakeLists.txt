# Contest 2026 team 148 - VelaGuard
#
# NUTTX_BOARD_DIR is overwritten by the custom board dir (Huangshan Pi BSP)
# when CONFIG_ARCH_BOARD_CUSTOM=y.  Contest-specific board sources live in
# vendor/openvela/boards/contest2026_148_board/src/ and must be referenced
# explicitly.

set(CONTEST2026_148_BOARD_DIR
    ${NUTTX_DIR}/../vendor/openvela/boards/contest2026_148_board)

set(CONTEST2026_148_BT_DEP)
if(CONFIG_BLUETOOTH)
  set(CONTEST2026_148_BT_DEP libbluetooth)
endif()

file(GLOB VELAGUARD_UI_SRCS
     ui/velaguard_font_30.c
     ui/velaguard_img_*.c)

# Noise suppression (libspeexdsp preprocess, BSD-3 licensed, vendored under
# audio/speexdsp).  Enabled by CONFIG_CONTEST2026_148_DENOISE.
# The speexdsp sources are compiled ONCE into a shared static library that
# both the velaguard and audio_test applications link against.
set(CONTEST2026_148_DENOISE_DEP)
if(CONFIG_CONTEST2026_148_DENOISE)
  nuttx_add_library(libvg_speexdsp STATIC)
  target_sources(libvg_speexdsp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/vg_denoise.c
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/speexdsp/preprocess.c
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/speexdsp/filterbank.c
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/speexdsp/kiss_fft.c
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/speexdsp/kiss_fftr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/speexdsp/fftwrap.c)
  target_include_directories(libvg_speexdsp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/speexdsp
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/speexdsp/include)
  target_compile_definitions(libvg_speexdsp PRIVATE
    OUTSIDE_SPEEX
    FIXED_POINT
    USE_KISS_FFT
    RANDOM_PREFIX=nuttx
    VG_DENOISE_NO_ECHO)
  set(CONTEST2026_148_DENOISE_DEP libvg_speexdsp)
endif()

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
    ${CONTEST2026_148_BT_DEP}
    ${CONTEST2026_148_DENOISE_DEP}
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
    ${NUTTX_DIR}/../frameworks/connectivity/bluetooth/framework/include
    ${NUTTX_DIR}/../apps/system/libuv/libuv/include
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
    audio/velaguard_audio_task.c
    audio/velaguard_wav_parser.c
    sensors/velaguard_imu.c
    comm/velaguard_ble.c
    comm/velaguard_ble_compat.c
    ${VELAGUARD_UI_SRCS})

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
      DEPENDS
      ${CONTEST2026_148_DENOISE_DEP}
      SRCS
      audio/audio_test.c)
  endif()
endif()
