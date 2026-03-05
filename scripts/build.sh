#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

BOARD="${BOARD:-lckfb_sky_board_stm32f407}"
APP_DIR="${APP_DIR:-${PROJECT_DIR}}"
BUILD_DIR="${BUILD_DIR:-${PROJECT_DIR}/build/${BOARD}}"
PRISTINE="${PRISTINE:-auto}"

find_west_topdir() {
  local dir="$1"
  while [[ "$dir" != "/" ]]; do
    if [[ -d "$dir/.west" ]]; then
      echo "$dir"
      return 0
    fi
    dir="$(dirname "$dir")"
  done
  return 1
}

if ! command -v west >/dev/null 2>&1; then
  echo "error: west not found in PATH" >&2
  echo "hint: activate your Zephyr venv first" >&2
  exit 127
fi

if ! WEST_TOPDIR="$(find_west_topdir "${PROJECT_DIR}")"; then
  echo "error: could not find Zephyr workspace (.west) from ${PROJECT_DIR}" >&2
  exit 2
fi

cd "${WEST_TOPDIR}"
exec west build -b "${BOARD}" "${APP_DIR}" -d "${BUILD_DIR}" -p "${PRISTINE}" "$@"
