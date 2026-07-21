# VelaGuard keeps its Huangshan Pi configuration in configs/nsh.
# The actual board source is selected by CONFIG_ARCH_BOARD_CUSTOM_DIR and
# remains the official SiFli Huangshan Pi BSP.
if(CONFIG_ARCH_BOARD_CONTEST2026_000_BOARD)
  add_subdirectory(src)
endif()
