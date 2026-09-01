#!/bin/bash
############################################################################
# contest2026_148_langyongyunji/scripts/build.sh
#
# Wrapper build script that:
#   1. Applies audio patches to vendor/sifli (from feature/sf32lb52-audio-codec)
#   2. Configures and builds the VelaGuard firmware
#
# Usage:
#   ./scripts/build.sh              # full build (apply patches + compile)
#   ./scripts/build.sh --rebuild    # reconfigure and build
#   ./scripts/build.sh --no-patch   # skip patch application
############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${REPO_ROOT}/cmake_out/velaguard_huangshan"
CONTEST_DIR="${REPO_ROOT}/contest2026_148_langyongyunji"

DO_PATCH=true
DO_RECONFIG=false

for arg in "$@"; do
  case "$arg" in
    --no-patch) DO_PATCH=false ;;
    --rebuild)  DO_RECONFIG=true ;;
    --help|-h)
      echo "Usage: $0 [--rebuild] [--no-patch]"
      exit 0
      ;;
    *)
      echo "Unknown option: $arg"
      echo "Usage: $0 [--rebuild] [--no-patch]"
      exit 1
      ;;
  esac
done

# --- Step 1: Apply patches ---
if $DO_PATCH; then
  echo "=== [build.sh] Applying audio patches ==="
  "${CONTEST_DIR}/scripts/apply_patches.sh"
fi

# --- Step 2: Configure (if needed) ---
if $DO_RECONFIG || [[ ! -f "${BUILD_DIR}/build.ninja" ]]; then
  echo "=== [build.sh] Configuring build ==="
  export PYTHONPATH="${REPO_ROOT}/prebuilts/tools/python/dist-packages/pyelftools:${REPO_ROOT}/prebuilts/tools/python/dist-packages/cxxfilt:${PYTHONPATH:-}"
  export PATH="${REPO_ROOT}/prebuilts/tools/linux/x86_64:${PATH:-}"

  CMAKE_RECONFIG_ARGS=()
  if $DO_RECONFIG; then
    CMAKE_RECONFIG_ARGS=(-U NUTTX_DEFCONFIG_SAVED)
  fi

  cmake "${CMAKE_RECONFIG_ARGS[@]}" -B "${BUILD_DIR}" -S "${REPO_ROOT}/nuttx" -GNinja \
    -DBOARD_CONFIG="${CONTEST_DIR}/board/contest_board/configs/nsh" \
    -DKCONFIG_CONFIG="${CONTEST_DIR}/board/contest_board/configs/nsh/defconfig" \
    -DEXTRA_FLAGS="-Wno-cpp -Wno-deprecated-declarations"
fi

# --- Step 3: Build ---
echo "=== [build.sh] Building ==="
cmake --build "${BUILD_DIR}" --target nuttx.bin

echo "=== [build.sh] Done: ${BUILD_DIR}/nuttx.bin ==="
