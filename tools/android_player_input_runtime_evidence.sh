#!/usr/bin/env bash
set -euo pipefail

PACKAGE='org.nevergone.recomp'
REMOTE_UI_XML='/sdcard/nevergone-player-input-evidence.xml'
EVIDENCE_DIR="${NEVERGONE_EVIDENCE_DIR:-evidence/android-player-input-runtime}"
LOCAL_UI_XML="$EVIDENCE_DIR/runtime-window.xml"
BEFORE_DIAGNOSTICS="$EVIDENCE_DIR/before-diagnostics.txt"
AFTER_DIAGNOSTICS="$EVIDENCE_DIR/after-diagnostics.txt"
BEFORE_SCREENSHOT="$EVIDENCE_DIR/before.png"
AFTER_SCREENSHOT="$EVIDENCE_DIR/after.png"
REPORT="$EVIDENCE_DIR/runtime-report.txt"

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
  echo 'Never Gone Recomp is not running. Reach the first GameLevels scene before capturing input evidence.' >&2
  exit 1
fi

activity_state="$(timeout 10s adb shell dumpsys activity activities 2>/dev/null || true)"
if ! grep -Eq \
    'mResumedActivity.*org\.nevergone\.recomp|topResumedActivity=.*org\.nevergone\.recomp' \
    <<<"$activity_state"; then
  echo 'Never Gone Recomp is not the resumed Activity.' >&2
  exit 1
fi

mkdir -p "$EVIDENCE_DIR"

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

refresh_and_extract() {
  local output="$1"
  local center tap_x tap_y attempt

  if ! center="$(find_refresh_center)"; then
    echo 'Could not locate Refresh runtime diagnostics without leaving the current Activity.' >&2
    return 1
  fi
  read -r tap_x tap_y <<<"$center"
  timeout 5s adb shell input tap "$tap_x" "$tap_y" >/dev/null
  sleep 1

  for attempt in 1 2 3 4 5; do
    if dump_runtime_ui &&
        python3 tools/android_game_scene_runtime_evidence.py \
          extract-ui "$LOCAL_UI_XML" "$output" >/dev/null 2>&1; then
      return 0
    fi
    scroll_runtime_ui toward-end || true
    sleep 1
  done
  return 1
}

capture_png() {
  local output="$1"
  timeout 10s adb exec-out screencap -p >"$output"
  [[ -s "$output" ]]
}

write_report() {
  local status="$1"
  local direction="$2"
  local swipe="$3"
  {
    printf 'status=%s\n' "$status"
    printf 'timestamp_utc=%s\n' "$(date -u +'%Y-%m-%dT%H:%M:%SZ')"
    printf 'direction=%s\n' "$direction"
    printf 'surface_swipe=%s\n' "$swipe"
    printf 'before_diagnostics_sha256=%s\n' "$(sha256sum "$BEFORE_DIAGNOSTICS" | awk '{print $1}')"
    printf 'after_diagnostics_sha256=%s\n' "$(sha256sum "$AFTER_DIAGNOSTICS" | awk '{print $1}')"
    printf 'before_screenshot_sha256=%s\n' "$(sha256sum "$BEFORE_SCREENSHOT" | awk '{print $1}')"
    printf 'after_screenshot_sha256=%s\n' "$(sha256sum "$AFTER_SCREENSHOT" | awk '{print $1}')"
  } >"$REPORT"
}

if ! refresh_and_extract "$BEFORE_DIAGNOSTICS"; then
  echo 'Could not retain baseline GameScene diagnostics.' >&2
  exit 1
fi

if ! direction="$(python3 tools/android_player_input_runtime_evidence.py \
    choose-direction "$BEFORE_DIAGNOSTICS")"; then
  echo 'Baseline diagnostics do not expose a usable player-control/motion state.' >&2
  exit 1
fi

if ! dump_runtime_ui; then
  echo 'Could not capture GameSurfaceView bounds before the test gesture.' >&2
  exit 1
fi
if ! swipe="$(python3 tools/android_player_input_runtime_evidence.py \
    surface-swipe "$LOCAL_UI_XML" "$direction")"; then
  echo 'Could not derive the recovered joystick swipe from GameSurfaceView bounds.' >&2
  exit 1
fi
read -r start_x start_y end_x end_y <<<"$swipe"

if ! capture_png "$BEFORE_SCREENSHOT"; then
  echo 'Baseline screenshot capture returned no data.' >&2
  exit 1
fi

# A 600 ms swipe supplies DOWN/MOVE/UP through Android's normal input transport.
# Start and end points are the recovered fixed joystick center +/-100 design units,
# safely inside its 165 begin radius and 290 update radius.
timeout 5s adb shell input swipe "$start_x" "$start_y" "$end_x" "$end_y" 600 >/dev/null
sleep 0.2

if ! capture_png "$AFTER_SCREENSHOT"; then
  echo 'Post-gesture screenshot capture returned no data.' >&2
  exit 1
fi

if ! refresh_and_extract "$AFTER_DIAGNOSTICS"; then
  echo 'Could not retain post-gesture GameScene diagnostics.' >&2
  exit 1
fi

if ! python3 tools/android_player_input_runtime_evidence.py \
    validate "$BEFORE_DIAGNOSTICS" "$AFTER_DIAGNOSTICS"; then
  write_report fail "$direction" "$swipe"
  echo "Player input/motion proof is incomplete; evidence retained in $EVIDENCE_DIR" >&2
  exit 1
fi

write_report pass "$direction" "$swipe"
echo "Player input/motion runtime evidence captured: $EVIDENCE_DIR"
