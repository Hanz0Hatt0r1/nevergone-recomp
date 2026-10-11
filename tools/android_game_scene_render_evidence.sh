#!/usr/bin/env bash
set -euo pipefail

PACKAGE='org.nevergone.recomp'
REMOTE_UI_XML='/sdcard/nevergone-game-scene-evidence.xml'
EVIDENCE_DIR="${NEVERGONE_EVIDENCE_DIR:-artifacts/android-game-scene-runtime}"
LOCAL_UI_XML=''
DIAGNOSTICS=''
SCREENSHOT=''
REPORT=''

for command in adb python3 sha256sum timeout; do
  if ! command -v "$command" >/dev/null 2>&1; then
    echo "Required command is missing: $command" >&2
    exit 2
  fi
done

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

timeout 30s adb wait-for-device

if ! timeout 5s adb shell pidof "$PACKAGE" | grep -q '[0-9]'; then
  echo 'Never Gone Recomp is not running. Reach the first GameLevels scene before capturing evidence.' >&2
  exit 1
fi

activity_state="$(timeout 10s adb shell dumpsys activity activities 2>/dev/null || true)"
if ! grep -Eq \
    'mResumedActivity.*org\.nevergone\.recomp|topResumedActivity=.*org\.nevergone\.recomp' \
    <<<"$activity_state"; then
  echo 'Never Gone Recomp is not the resumed Activity. Return to the live first scene before capture.' >&2
  exit 1
fi

mkdir -p "$EVIDENCE_DIR"
LOCAL_UI_XML="$EVIDENCE_DIR/runtime-window.xml"
DIAGNOSTICS="$EVIDENCE_DIR/runtime-diagnostics.txt"
SCREENSHOT="$EVIDENCE_DIR/scene.png"
REPORT="$EVIDENCE_DIR/runtime-report.txt"

cleanup() {
  timeout 5s adb shell rm -f "$REMOTE_UI_XML" >/dev/null 2>&1 || true
}
trap cleanup EXIT

dump_runtime_ui() {
  rm -f "$LOCAL_UI_XML"
  timeout 5s adb shell rm -f "$REMOTE_UI_XML" >/dev/null 2>&1 || true
  timeout 15s adb shell uiautomator dump "$REMOTE_UI_XML" >/dev/null
  timeout 5s adb exec-out cat "$REMOTE_UI_XML" >"$LOCAL_UI_XML"
  [[ -s "$LOCAL_UI_XML" ]] && grep -Fq "package=\"$PACKAGE\"" "$LOCAL_UI_XML"
}

scroll_runtime_ui() {
  local direction="$1"
  local swipe start_x start_y end_x end_y

  if ! swipe="$(python3 tools/android_runtime_diagnostics.py scroll-swipe "$LOCAL_UI_XML" 2>/dev/null)"; then
    return 1
  fi
  read -r start_x start_y end_x end_y <<<"$swipe"
  [[ -n "$start_x" && -n "$start_y" && -n "$end_x" && -n "$end_y" ]]
  if [[ "$direction" == 'toward-start' ]]; then
    timeout 5s adb shell input swipe "$end_x" "$end_y" "$start_x" "$start_y" 250 >/dev/null
  else
    timeout 5s adb shell input swipe "$start_x" "$start_y" "$end_x" "$end_y" 250 >/dev/null
  fi
}

find_refresh_center() {
  local direction attempt center
  for direction in toward-start toward-end; do
    for attempt in 1 2 3 4 5; do
      if ! dump_runtime_ui; then
        echo "Could not capture Nevergone UI while locating diagnostics (${direction} ${attempt}/5)" >&2
        sleep 1
        continue
      fi
      if center="$(python3 tools/android_runtime_diagnostics.py refresh-center "$LOCAL_UI_XML" 2>/dev/null)"; then
        printf '%s\n' "$center"
        return 0
      fi
      scroll_runtime_ui "$direction" || true
      sleep 1
    done
  done
  return 1
}

if ! center="$(find_refresh_center)"; then
  echo 'Could not locate Refresh runtime diagnostics without leaving the current Activity.' >&2
  exit 1
fi
read -r tap_x tap_y <<<"$center"
timeout 5s adb shell input tap "$tap_x" "$tap_y" >/dev/null
sleep 1

extracted=0
for attempt in 1 2 3 4 5; do
  if dump_runtime_ui &&
      python3 tools/android_game_scene_runtime_evidence.py \
        extract-ui "$LOCAL_UI_XML" "$DIAGNOSTICS" >/dev/null 2>&1; then
    extracted=1
    break
  fi
  scroll_runtime_ui toward-end || true
  sleep 1
done
if (( extracted == 0 )); then
  echo 'Could not extract GameScene renderer/staging diagnostics from the current Activity.' >&2
  exit 1
fi

# Capture the actual screen after refreshing diagnostics. The GLSurfaceView
# remains visible above the diagnostics ScrollView, so this retains the visual
# artifact used to review the first static GameScene proof.
timeout 10s adb exec-out screencap -p >"$SCREENSHOT"
if [[ ! -s "$SCREENSHOT" ]]; then
  echo 'Android screenshot capture returned no data.' >&2
  exit 1
fi

getprop_value() {
  timeout 5s adb shell getprop "$1" 2>/dev/null | tr -d '\r'
}

ANDROID_RELEASE="$(getprop_value ro.build.version.release)"
ANDROID_SDK="$(getprop_value ro.build.version.sdk | tr -d '[:space:]')"
MANUFACTURER="$(getprop_value ro.product.manufacturer)"
MODEL="$(getprop_value ro.product.model)"
DEVICE="$(getprop_value ro.product.device)"
PRIMARY_ABI="$(getprop_value ro.product.cpu.abi | tr -d '[:space:]')"
PAGE_SIZE="$(timeout 5s adb shell getconf PAGE_SIZE 2>/dev/null | tr -d '\r[:space:]')"
DIAGNOSTICS_SHA="$(sha256sum "$DIAGNOSTICS" | awk '{print $1}')"
SCREENSHOT_SHA="$(sha256sum "$SCREENSHOT" | awk '{print $1}')"

write_report() {
  local status="$1"
  {
    printf 'status=%s\n' "$status"
    printf 'timestamp_utc=%s\n' "$(date -u +'%Y-%m-%dT%H:%M:%SZ')"
    printf 'android_release=%s\n' "$ANDROID_RELEASE"
    printf 'android_sdk=%s\n' "$ANDROID_SDK"
    printf 'manufacturer=%s\n' "$MANUFACTURER"
    printf 'model=%s\n' "$MODEL"
    printf 'device=%s\n' "$DEVICE"
    printf 'primary_abi=%s\n' "$PRIMARY_ABI"
    printf 'page_size=%s\n' "$PAGE_SIZE"
    printf 'diagnostics_file=%s\n' "$(basename "$DIAGNOSTICS")"
    printf 'diagnostics_sha256=%s\n' "$DIAGNOSTICS_SHA"
    printf 'screenshot_file=%s\n' "$(basename "$SCREENSHOT")"
    printf 'screenshot_sha256=%s\n' "$SCREENSHOT_SHA"
  } >"$REPORT"
}

write_report pass
if ! python3 tools/android_game_scene_runtime_evidence.py validate "$REPORT"; then
  write_report fail
  echo "First-scene render proof is not complete; diagnostic evidence retained in $EVIDENCE_DIR" >&2
  exit 1
fi

echo "First-scene render evidence captured: $EVIDENCE_DIR"
