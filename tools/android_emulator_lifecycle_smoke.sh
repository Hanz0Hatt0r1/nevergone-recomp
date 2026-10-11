#!/usr/bin/env bash
set -euo pipefail

PACKAGE='org.nevergone.recomp'
COMPONENT='org.nevergone.recomp/.MainActivity'
REMOTE_UI_XML='/sdcard/nevergone-runtime-window.xml'
LOCAL_UI_XML="$(mktemp)"
APK="$(find android/app/build/outputs/apk/debug -name '*.apk' -print -quit)"

capture_failure_diagnostics() {
  local activity_output

  echo '--- Never Gone Recomp lifecycle failure diagnostics ---' >&2
  printf 'android_sdk=%s\n' "$(timeout 5s adb shell getprop ro.build.version.sdk 2>/dev/null | tr -d '\r' || true)" >&2
  printf 'primary_abi=%s\n' "$(timeout 5s adb shell getprop ro.product.cpu.abi 2>/dev/null | tr -d '\r' || true)" >&2
  printf 'page_size=%s\n' "$(timeout 5s adb shell getconf PAGE_SIZE 2>/dev/null | tr -d '\r[:space:]' || true)" >&2

  echo '--- adb devices ---' >&2
  timeout 5s adb devices -l >&2 || true

  echo '--- bounded ActivityManager state ---' >&2
  set +e
  activity_output="$(timeout 10s adb shell dumpsys activity activities 2>&1)"
  set -e
  printf '%s\n' "$activity_output" |
    grep -E -C 4 \
      'org\.nevergone\.recomp|mResumedActivity|topResumedActivity|DUMP TIMEOUT|Broken pipe|Can.t find service|Failure calling service activity' |
    tail -n 120 >&2 || true

  echo '--- bounded Nevergone/AndroidRuntime logcat ---' >&2
  timeout 10s adb logcat -d -t 400 2>&1 |
    grep -E 'org\.nevergone\.recomp|AndroidRuntime|FATAL EXCEPTION|Fatal signal' |
    tail -n 160 >&2 || true

  echo '--- bounded package state ---' >&2
  timeout 10s adb shell dumpsys package "$PACKAGE" 2>&1 |
    grep -E 'versionCode=|versionName=|primaryCpuAbi=|secondaryCpuAbi=|pkgFlags=|stopped=' |
    tail -n 80 >&2 || true
  echo '--- end lifecycle failure diagnostics ---' >&2
}

cleanup() {
  local status=$?
  trap - EXIT

  if (( status != 0 )); then
    capture_failure_diagnostics || true
  fi

  rm -f "$LOCAL_UI_XML"
  timeout 5s adb shell rm -f "$REMOTE_UI_XML" >/dev/null 2>&1 || true
  exit "$status"
}
trap cleanup EXIT

if [[ -z "$APK" ]]; then
  echo 'Never Gone Recomp debug APK was not found' >&2
  exit 1
fi

wait_for_android_services() {
  adb wait-for-device
  local attempt
  for attempt in $(seq 1 60); do
    if [[ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == '1' ]] &&
        adb shell cmd package list packages >/dev/null 2>&1 &&
        adb shell true >/dev/null 2>&1; then
      sleep 2
      if adb shell cmd package list packages >/dev/null 2>&1 &&
          adb shell true >/dev/null 2>&1; then
        return 0
      fi
    fi
    sleep 2
  done

  echo 'Android services did not become stable before lifecycle smoke' >&2
  adb devices -l >&2 || true
  exit 1
}

activity_dump() {
  local output
  local status

  set +e
  output="$(timeout 20s adb shell dumpsys activity activities 2>&1)"
  status=$?
  set -e

  if [[ $status -ne 0 ]] || grep -Eqi \
      'DUMP TIMEOUT|Broken pipe|device offline|device .* not found|no devices/emulators found|Failure calling service activity|Can.t find service: activity|connection.*closed' \
      <<<"$output"; then
    return 1
  fi

  printf '%s\n' "$output"
}

wait_for_activity_service() {
  local phase="$1"
  local attempt

  if ! timeout 30s adb wait-for-device; then
    echo "Android device did not reconnect during ${phase}" >&2
    return 1
  fi

  for attempt in $(seq 1 30); do
    if [[ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == '1' ]] &&
        activity_dump >/dev/null; then
      return 0
    fi
    sleep 2
  done

  echo "Android activity service did not recover during ${phase}" >&2
  adb devices -l >&2 || true
  return 1
}

start_activity() {
  local phase="$1"
  local attempt
  local output
  local status

  for attempt in 1 2 3; do
    set +e
    output="$(adb shell am start -W -n "$COMPONENT" 2>&1)"
    status=$?
    set -e

    printf '%s\n' "$output"
    if [[ $status -eq 0 ]] && ! grep -Eqi '^Status:[[:space:]]*timeout|DUMP TIMEOUT' <<<"$output"; then
      return 0
    fi

    if [[ $status -eq 0 ]] && grep -Eqi '^Status:[[:space:]]*timeout|DUMP TIMEOUT' <<<"$output"; then
      :
    elif ! grep -Eqi \
        'Broken pipe|device offline|device .* not found|no devices/emulators found|Failure calling service activity|Can.t find service: activity|connection.*closed' \
        <<<"$output"; then
      echo "Activity start failed during ${phase} (status ${status})" >&2
      return "${status:-1}"
    fi

    if [[ $attempt -eq 3 ]]; then
      break
    fi

    echo "Transient Android activity-service failure during ${phase}; retrying (${attempt}/3)" >&2
    wait_for_activity_service "$phase"
  done

  echo "Android activity service remained unavailable during ${phase}" >&2
  return 1
}

fatal_check() {
  local phase="$1"
  if adb logcat -d | grep -E 'FATAL EXCEPTION|Fatal signal.*org\.nevergone\.recomp'; then
    echo "Never Gone Recomp crashed during ${phase}" >&2
    exit 1
  fi
}

require_pid() {
  local phase="$1"
  local pid
  pid="$(adb shell pidof "$PACKAGE" | tr -d '\r')"
  if [[ -z "$pid" ]]; then
    echo "Never Gone Recomp has no live process during ${phase}" >&2
    exit 1
  fi
  printf '%s' "$pid"
}

require_foreground() {
  local phase="$1"
  local attempt
  local output

  for attempt in 1 2 3; do
    if output="$(activity_dump)"; then
      if grep -Eq \
          'mResumedActivity.*org\.nevergone\.recomp|topResumedActivity=.*org\.nevergone\.recomp' \
          <<<"$output"; then
        return 0
      fi

      echo "Never Gone Recomp is not resumed during ${phase}" >&2
      printf '%s\n' "$output" >&2
      exit 1
    fi

    if [[ $attempt -eq 3 ]]; then
      break
    fi

    echo "Transient ActivityManager dump failure during ${phase}; retrying (${attempt}/3)" >&2
    wait_for_activity_service "$phase"
  done

  echo "Android activity service remained unavailable while checking foreground during ${phase}" >&2
  adb devices -l >&2 || true
  exit 1
}

dump_runtime_ui() {
  timeout 15s adb shell uiautomator dump "$REMOTE_UI_XML" >/dev/null
  timeout 5s adb exec-out cat "$REMOTE_UI_XML" >"$LOCAL_UI_XML"
  [[ -s "$LOCAL_UI_XML" ]]
}

print_runtime_ui_excerpt() {
  echo '--- bounded uiautomator XML excerpt ---' >&2
  head -c 8192 "$LOCAL_UI_XML" >&2 || true
  echo >&2
  echo '--- end uiautomator XML excerpt ---' >&2
}

scroll_runtime_ui() {
  local direction="${1:-toward-end}"
  local swipe
  local start_x
  local start_y
  local end_x
  local end_y

  if ! swipe="$(python3 tools/android_runtime_diagnostics.py scroll-swipe "$LOCAL_UI_XML" --direction "$direction" 2>/dev/null)"; then
    return 1
  fi
  read -r start_x start_y end_x end_y <<<"$swipe"
  [[ -n "$start_x" && -n "$start_y" && -n "$end_x" && -n "$end_y" ]]
  timeout 5s adb shell input swipe "$start_x" "$start_y" "$end_x" "$end_y" 250 >/dev/null
}

find_refresh_center() {
  local phase="$1"
  local pass
  local attempt
  local direction
  local center

  for pass in toward-start toward-end; do
    direction="$pass"
    for attempt in 1 2 3 4 5; do
      if ! dump_runtime_ui; then
        echo "Could not dump runtime UI during ${phase} (${direction} ${attempt}/5)" >&2
        sleep 1
        continue
      fi
      if center="$(python3 tools/android_runtime_diagnostics.py refresh-center "$LOCAL_UI_XML" 2>/dev/null)"; then
        printf '%s\n' "$center"
        return 0
      fi

      echo "Refresh diagnostics control is outside the current UI viewport during ${phase}; scrolling ${direction} (${attempt}/5)" >&2
      scroll_runtime_ui "$direction" || true
      sleep 1
    done
  done

  # Check once more after the final bounded swipe so a control that entered the
  # viewport on that gesture is not missed.
  if dump_runtime_ui && center="$(python3 tools/android_runtime_diagnostics.py refresh-center "$LOCAL_UI_XML" 2>/dev/null)"; then
    printf '%s\n' "$center"
    return 0
  fi

  echo "Could not locate Refresh runtime diagnostics during ${phase}" >&2
  python3 tools/android_runtime_diagnostics.py refresh-center "$LOCAL_UI_XML" >&2 || true
  print_runtime_ui_excerpt
  return 1
}

read_renderer_counters() {
  local phase="$1"
  local attempt
  local counters

  for attempt in 1 2 3 4 5; do
    if ! dump_runtime_ui; then
      echo "Could not dump renderer diagnostics during ${phase} (${attempt}/5)" >&2
      sleep 1
      continue
    fi
    if counters="$(python3 tools/android_runtime_diagnostics.py renderer-counters "$LOCAL_UI_XML" 2>/dev/null)"; then
      printf '%s\n' "$counters"
      return 0
    fi

    scroll_runtime_ui toward-end || true
    sleep 1
  done

  echo "Could not read renderer lifecycle counters during ${phase}" >&2
  python3 tools/android_runtime_diagnostics.py renderer-counters "$LOCAL_UI_XML" >&2 || true
  print_runtime_ui_excerpt
  return 1
}

refresh_runtime_counters() {
  local phase="$1"
  local center
  local counters
  local tap_x
  local tap_y

  if ! center="$(find_refresh_center "$phase")"; then
    return 1
  fi
  read -r tap_x tap_y <<<"$center"
  if [[ -z "$tap_x" || -z "$tap_y" ]]; then
    echo "Refresh diagnostics coordinates are empty during ${phase}" >&2
    return 1
  fi
  timeout 5s adb shell input tap "$tap_x" "$tap_y"

  if ! counters="$(read_renderer_counters "$phase")"; then
    return 1
  fi
  echo "renderer lifecycle counters (${phase}): ${counters}" >&2
  printf '%s\n' "$counters"
}

capture_runtime_counters() {
  local phase="$1"
  local counters

  if ! counters="$(refresh_runtime_counters "$phase")"; then
    echo "Runtime diagnostics refresh failed during ${phase}" >&2
    return 1
  fi
  printf '%s\n' "$counters"
}

assert_gt() {
  local newer="$1"
  local older="$2"
  local label="$3"
  if (( newer <= older )); then
    echo "Expected ${label} to increase: previous=${older}, current=${newer}" >&2
    exit 1
  fi
}

wait_for_android_services
adb install -r "$APK"
adb logcat -c

start_activity 'initial launch'
sleep 5
INITIAL_PID="$(require_pid 'initial launch')"
require_foreground 'initial launch'
fatal_check 'initial launch'
if ! INITIAL_COUNTERS="$(capture_runtime_counters 'initial launch')"; then
  exit 1
fi
read -r INITIAL_RESUME INITIAL_PAUSE INITIAL_SURFACE <<<"$INITIAL_COUNTERS"
(( INITIAL_RESUME >= 1 ))
(( INITIAL_SURFACE >= 1 ))

adb shell input keyevent KEYCODE_HOME
sleep 2
PAUSED_PID="$(require_pid 'home pause')"
[[ "$PAUSED_PID" == "$INITIAL_PID" ]]
fatal_check 'home pause'

start_activity 'resume'
sleep 3
RESUMED_PID="$(require_pid 'resume')"
[[ "$RESUMED_PID" == "$INITIAL_PID" ]]
require_foreground 'resume'
fatal_check 'resume'
if ! RESUMED_COUNTERS="$(capture_runtime_counters 'resume')"; then
  exit 1
fi
read -r RESUMED_RESUME RESUMED_PAUSE RESUMED_SURFACE <<<"$RESUMED_COUNTERS"
assert_gt "$RESUMED_RESUME" "$INITIAL_RESUME" 'resume events after Home/resume'
assert_gt "$RESUMED_PAUSE" "$INITIAL_PAUSE" 'pause events after Home/resume'

adb shell input keyevent KEYCODE_BACK
sleep 2
RECREATE_PID="$(require_pid 'activity finish')"
[[ "$RECREATE_PID" == "$INITIAL_PID" ]]
start_activity 'activity recreation'
sleep 3
RECREATED_PID="$(require_pid 'activity recreation')"
[[ "$RECREATED_PID" == "$INITIAL_PID" ]]
require_foreground 'activity recreation'
fatal_check 'activity recreation'
if ! RECREATED_COUNTERS="$(capture_runtime_counters 'activity recreation')"; then
  exit 1
fi
read -r RECREATED_RESUME RECREATED_PAUSE RECREATED_SURFACE <<<"$RECREATED_COUNTERS"
assert_gt "$RECREATED_RESUME" "$RESUMED_RESUME" 'resume events after Activity recreation'
assert_gt "$RECREATED_PAUSE" "$RESUMED_PAUSE" 'pause events after Activity recreation'
assert_gt "$RECREATED_SURFACE" "$RESUMED_SURFACE" 'GL surface generation after Activity recreation'

adb shell am force-stop "$PACKAGE"
sleep 1
if adb shell pidof "$PACKAGE" | grep -q '[0-9]'; then
  echo 'Never Gone Recomp process survived force-stop' >&2
  exit 1
fi
start_activity 'cold restart'
sleep 3
require_pid 'cold restart' >/dev/null
require_foreground 'cold restart'
fatal_check 'cold restart'
if ! COLD_COUNTERS="$(capture_runtime_counters 'cold restart')"; then
  exit 1
fi
read -r COLD_RESUME COLD_PAUSE COLD_SURFACE <<<"$COLD_COUNTERS"
(( COLD_RESUME >= 1 ))
(( COLD_SURFACE >= 1 ))

adb shell am force-stop "$PACKAGE"
