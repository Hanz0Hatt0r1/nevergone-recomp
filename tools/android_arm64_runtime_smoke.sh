#!/usr/bin/env bash
set -euo pipefail

PACKAGE='org.nevergone.recomp'
DEFAULT_EVIDENCE_DIR='artifacts/android-arm64-runtime'
APK="${1:-${NEVERGONE_APK:-}}"
EVIDENCE_DIR="${NEVERGONE_EVIDENCE_DIR:-$DEFAULT_EVIDENCE_DIR}"

if [[ -z "$APK" || ! -f "$APK" ]]; then
  echo 'Usage: tools/android_arm64_runtime_smoke.sh /path/to/nevergone.apk' >&2
  exit 2
fi

for command in adb unzip sha256sum realpath python3 timeout; do
  if ! command -v "$command" >/dev/null 2>&1; then
    echo "Required command is missing: $command" >&2
    exit 2
  fi
done

APK="$(realpath "$APK")"

if [[ -z "${ANDROID_SERIAL:-}" ]]; then
  mapfile -t connected_devices < <(
    timeout 10s adb devices | awk 'NR > 1 && $2 == "device" { print $1 }'
  )
  if (( ${#connected_devices[@]} != 1 )); then
    echo 'Connect exactly one adb device, or set ANDROID_SERIAL explicitly.' >&2
    timeout 5s adb devices -l >&2 || true
    exit 2
  fi
  export ANDROID_SERIAL="${connected_devices[0]}"
fi

if ! timeout 30s adb wait-for-device; then
  echo 'Android device did not become available within the runtime-evidence window.' >&2
  exit 2
fi

adb_getprop() {
  local key="$1"
  timeout 5s adb shell getprop "$key" 2>/dev/null | tr -d '\r'
}

SDK="$(adb_getprop ro.build.version.sdk | tr -d '[:space:]')"
ANDROID_RELEASE="$(adb_getprop ro.build.version.release)"
ABI="$(adb_getprop ro.product.cpu.abi | tr -d '[:space:]')"
ABI_LIST="$(adb_getprop ro.product.cpu.abilist | tr -d '[:space:]')"
PAGE_SIZE="$(timeout 5s adb shell getconf PAGE_SIZE 2>/dev/null | tr -d '\r[:space:]')"
MANUFACTURER="$(adb_getprop ro.product.manufacturer)"
MODEL="$(adb_getprop ro.product.model)"
DEVICE="$(adb_getprop ro.product.device)"

if [[ "$SDK" != '35' && "$SDK" != '36' ]]; then
  echo "Expected Android API 35 or 36, got SDK=${SDK:-<empty>}" >&2
  exit 1
fi
if [[ "$ABI" != 'arm64-v8a' ]]; then
  echo "Expected an arm64-v8a primary ABI, got ABI=${ABI:-<empty>}" >&2
  exit 1
fi
APK_ENTRIES="$(unzip -Z1 "$APK")"
if ! grep -Eq '^lib/arm64-v8a/[^/]+\.so$' <<<"$APK_ENTRIES"; then
  echo 'APK does not contain arm64-v8a native libraries.' >&2
  exit 1
fi

mkdir -p "$EVIDENCE_DIR"
REPORT="$EVIDENCE_DIR/runtime-report.txt"
TRANSCRIPT="$EVIDENCE_DIR/lifecycle-smoke.txt"
LOGCAT="$EVIDENCE_DIR/logcat-nevergone.txt"
ACTIVITY="$EVIDENCE_DIR/activity-nevergone.txt"
PACKAGE_INFO="$EVIDENCE_DIR/package-nevergone.txt"

write_report() {
  local status="$1"
  {
    printf 'status=%s\n' "$status"
    printf 'timestamp_utc=%s\n' "$(date -u +'%Y-%m-%dT%H:%M:%SZ')"
    printf 'android_release=%s\n' "$ANDROID_RELEASE"
    printf 'android_sdk=%s\n' "$SDK"
    printf 'manufacturer=%s\n' "$MANUFACTURER"
    printf 'model=%s\n' "$MODEL"
    printf 'device=%s\n' "$DEVICE"
    printf 'primary_abi=%s\n' "$ABI"
    printf 'abi_list=%s\n' "$ABI_LIST"
    printf 'page_size=%s\n' "$PAGE_SIZE"
    printf 'apk_sha256=%s\n' "$(sha256sum "$APK" | awk '{print $1}')"
    printf 'lifecycle_transcript=%s\n' "$(basename "$TRANSCRIPT")"
  } >"$REPORT"
}

capture_diagnostics() {
  timeout 10s adb logcat -d 2>/dev/null |
    grep -E 'org\.nevergone\.recomp|AndroidRuntime|FATAL EXCEPTION|Fatal signal' \
      >"$LOGCAT" || true
  timeout 10s adb shell dumpsys activity activities 2>/dev/null |
    grep -E -C 4 'org\.nevergone\.recomp' >"$ACTIVITY" || true
  timeout 10s adb shell dumpsys package "$PACKAGE" 2>/dev/null |
    grep -E 'versionCode=|versionName=|primaryCpuAbi=|secondaryCpuAbi=' \
      >"$PACKAGE_INFO" || true
}

sanitize_transcript() {
  local raw="$1"
  python3 - "$raw" "$TRANSCRIPT" "$ANDROID_SERIAL" <<'PY'
from pathlib import Path
import sys

source = Path(sys.argv[1])
target = Path(sys.argv[2])
serial = sys.argv[3]
text = source.read_text(encoding="utf-8", errors="replace")
if serial:
    text = text.replace(serial, "<adb-serial>")
target.write_text(text, encoding="utf-8")
PY
}

write_report running

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGING_ROOT="$(mktemp -d)"
RAW_TRANSCRIPT="$STAGING_ROOT/lifecycle-smoke.raw.txt"
cleanup() {
  rm -rf "$STAGING_ROOT"
}
trap cleanup EXIT
mkdir -p "$STAGING_ROOT/android/app/build/outputs/apk/debug"
ln -s "$APK" "$STAGING_ROOT/android/app/build/outputs/apk/debug/nevergone-arm64.apk"
ln -s "$REPO_ROOT/tools" "$STAGING_ROOT/tools"

set +e
(
  cd "$STAGING_ROOT"
  bash "$REPO_ROOT/tools/android_emulator_lifecycle_smoke.sh"
) >"$RAW_TRANSCRIPT" 2>&1
STATUS=$?
set -e

sanitize_transcript "$RAW_TRANSCRIPT"
cat "$TRANSCRIPT"
capture_diagnostics

if (( STATUS != 0 )); then
  write_report fail
  echo "arm64 runtime smoke failed; evidence: $EVIDENCE_DIR" >&2
  exit "$STATUS"
fi

write_report pass
if ! python3 "$REPO_ROOT/tools/android_runtime_evidence.py" "$REPORT"; then
  write_report fail
  echo "arm64 runtime smoke produced invalid evidence: $EVIDENCE_DIR" >&2
  exit 1
fi

echo "arm64 runtime smoke passed; evidence: $EVIDENCE_DIR"
