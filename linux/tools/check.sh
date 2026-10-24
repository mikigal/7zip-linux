#!/usr/bin/env bash
# Single entry point for verifying the Linux port: configure, build (including
# the upstream 7zz oracle via the upstream makefile) and run the tests.
#
# Usage: linux/tools/check.sh [options] [-- extra ctest args]
#   --no-wayland     skip tests that need the nested KWin session (label "wayland")
#   --asan           build and test with AddressSanitizer + UBSan in build/linux-asan
#   --build-dir DIR  use DIR instead of build/linux
#   --clean          remove the build directory first
#   -j N             parallel build and test jobs (default: number of CPUs)

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$ROOT/build/linux"
JOBS="$(nproc)"
CMAKE_ARGS=()
CTEST_ARGS=()
CLEAN=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-wayland) CTEST_ARGS+=(-LE wayland) ;;
    --asan)
      BUILD_DIR="$ROOT/build/linux-asan"
      CMAKE_ARGS+=(-DZ7_SANITIZE=address,undefined -DCMAKE_BUILD_TYPE=Debug)
      ;;
    --build-dir) BUILD_DIR="$(realpath -m "$2")"; shift ;;
    --clean) CLEAN=1 ;;
    -j) JOBS="$2"; shift ;;
    --) shift; CTEST_ARGS+=("$@"); break ;;
    -h|--help) sed -n '2,12p' "$0"; exit 0 ;;
    *) echo "check.sh: unknown option $1" >&2; exit 2 ;;
  esac
  shift
done

step() { printf '\n==> %s\n' "$*"; }

if [[ $CLEAN -eq 1 ]]; then
  step "Removing $BUILD_DIR"
  rm -rf "$BUILD_DIR"
fi

step "Configure ($BUILD_DIR)"
cmake -S "$ROOT/linux" -B "$BUILD_DIR" -G Ninja "${CMAKE_ARGS[@]}" >/dev/null

step "Build"
cmake --build "$BUILD_DIR" -j "$JOBS"

step "Test"
ctest --test-dir "$BUILD_DIR" --output-on-failure -j "$JOBS" "${CTEST_ARGS[@]}"

step "OK"
