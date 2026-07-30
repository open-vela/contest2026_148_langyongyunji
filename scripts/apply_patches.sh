#!/bin/bash
############################################################################
# contest2026_148_langyongyunji/scripts/apply_patches.sh
#
# Apply audio-related patches to vendor/sifli before compilation.
# Patches are taken from feature/sf32lb52-audio-codec and make the NuttX
# audio lower-half driver work on top of the dev-ai-contest-2026 baseline.
#
# Usage:
#   ./scripts/apply_patches.sh              # apply if not yet applied
#   ./scripts/apply_patches.sh --force      # re-apply even if already done
#   ./scripts/apply_patches.sh --check      # dry-run: check if patches apply
############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PATCH_DIR="${SCRIPT_DIR}/../patches/vendor_sifli"
SIFLI_DIR="${SCRIPT_DIR}/../../vendor/sifli"
STAMP_FILE="${SIFLI_DIR}/.audio_patches_applied"

MODE="apply"
if [[ "${1:-}" == "--force" ]]; then
  MODE="force"
elif [[ "${1:-}" == "--check" ]]; then
  MODE="check"
fi

# --- pre-flight checks ---
if [[ ! -d "${SIFLI_DIR}" ]]; then
  echo "[apply_patches] ERROR: vendor/sifli not found at ${SIFLI_DIR}" >&2
  exit 1
fi

if [[ ! -d "${PATCH_DIR}" ]]; then
  echo "[apply_patches] ERROR: patch directory not found at ${PATCH_DIR}" >&2
  exit 1
fi

PATCHES=($(ls "${PATCH_DIR}"/*.patch 2>/dev/null | sort))
if [[ ${#PATCHES[@]} -eq 0 ]]; then
  echo "[apply_patches] ERROR: no .patch files found in ${PATCH_DIR}" >&2
  exit 1
fi

# --- check mode ---
if [[ "${MODE}" == "check" ]]; then
  echo "[apply_patches] DRY-RUN: checking ${#PATCHES[@]} patch(es)..."
  for p in "${PATCHES[@]}"; do
    echo "  checking $(basename "$p") ..."
    if ! git -C "${SIFLI_DIR}" apply --check "$p"; then
      echo "[apply_patches] ERROR: $(basename "$p") does NOT apply cleanly" >&2
      exit 1
    fi
  done
  echo "[apply_patches] All patches apply cleanly."
  exit 0
fi

# --- detect if already applied ---
_KEY_FILE="${SIFLI_DIR}/chips/sf32lb52/sf32lb_audio.c"
if [[ "${MODE}" != "force" && -f "${_KEY_FILE}" ]]; then
  echo "[apply_patches] patches already applied (${_KEY_FILE} exists)"
  exit 0
fi

# --- apply ---
echo "[apply_patches] applying ${#PATCHES[@]} patch(es) to vendor/sifli..."
for p in "${PATCHES[@]}"; do
  echo "  applying $(basename "$p") ..."
  git -C "${SIFLI_DIR}" apply --whitespace=nowarn "$p"
done

echo "[apply_patches] done.  ${#PATCHES[@]} patch(es) applied successfully."
