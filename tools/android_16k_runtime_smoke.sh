#!/usr/bin/env bash
set -euo pipefail

PAGE_SIZE="$(adb shell getconf PAGE_SIZE | tr -d '\r[:space:]')"
ABI="$(adb shell getprop ro.product.cpu.abi | tr -d '\r[:space:]')"

if [[ "$PAGE_SIZE" != "16384" ]]; then
  echo "Expected a 16 KiB Android runtime, got PAGE_SIZE=${PAGE_SIZE:-<empty>}" >&2
  exit 1
fi
if [[ "$ABI" != "x86_64" ]]; then
  echo "Expected the 16 KiB CI transport image to be x86_64, got ABI=${ABI:-<empty>}" >&2
  exit 1
fi

echo "16 KiB Android runtime confirmed: page_size=${PAGE_SIZE}, abi=${ABI}"
exec bash tools/android_emulator_lifecycle_smoke.sh
