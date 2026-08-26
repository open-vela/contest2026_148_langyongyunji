#!/bin/bash
############################################################################
# contest2026_148_langyongyunji/scripts/apply_patches.sh
#
# Apply audio-related patches to vendor/sifli before compilation.
# Patches are taken from feature/sf32lb52-audio-codec and make the NuttX
# audio lower-half driver work on top of the dev-ai-contest-2026 baseline.
#
# A stamp file (scripts/.patches_applied, inside the contest repo) records
# which patches have already been applied, so vendor/sifli itself is never
# committed: patches stay here and get re-applied after a fresh repo sync.
#
# Usage:
#   ./scripts/apply_patches.sh              # apply not-yet-applied patches
#   ./scripts/apply_patches.sh --force      # re-apply all (requires a clean
#                                           #   vendor/sifli tree first)
#   ./scripts/apply_patches.sh --check      # dry-run: check if patches apply
############################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PATCH_DIR="${SCRIPT_DIR}/../patches/vendor_sifli"
SIFLI_DIR="${SCRIPT_DIR}/../../vendor/sifli"
STAMP_FILE="${SCRIPT_DIR}/.patches_applied"

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
    if ! git -C "${SIFLI_DIR}" apply --check --whitespace=nowarn "$p"; then
      echo "[apply_patches] ERROR: $(basename "$p") does NOT apply cleanly" >&2
      exit 1
    fi
  done
  echo "[apply_patches] All patches apply cleanly."
  exit 0
fi

# --- force mode: forget what was applied and re-apply everything ---
if [[ "${MODE}" == "force" ]]; then
  echo "[apply_patches] force mode: resetting patch stamp"
  rm -f "${STAMP_FILE}"
fi

# --- load stamp: which patches are already applied ---
declare -A APPLIED=()
if [[ -f "${STAMP_FILE}" ]]; then
  while IFS= read -r line; do
    [[ -n "$line" ]] && APPLIED["$line"]=1
  done < "${STAMP_FILE}"
fi

# --- determine patches still missing ---
TO_APPLY=()
for p in "${PATCHES[@]}"; do
  b="$(basename "$p")"
  if [[ -n "${APPLIED[$b]:-}" ]]; then
    echo "[apply_patches]   already applied: ${b}"
  else
    TO_APPLY+=("$p")
  fi
done

if [[ ${#TO_APPLY[@]} -eq 0 ]]; then
  echo "[apply_patches] all patches already applied"
  exit 0
fi

# --- apply ---
echo "[apply_patches] applying ${#TO_APPLY[@]} patch(es) to vendor/sifli..."
for p in "${TO_APPLY[@]}"; do
  echo "  applying $(basename "$p") ..."
  git -C "${SIFLI_DIR}" apply --whitespace=nowarn "$p"
  basename "$p" >> "${STAMP_FILE}"
done

echo "[apply_patches] done.  ${#TO_APPLY[@]} patch(es) applied successfully."
