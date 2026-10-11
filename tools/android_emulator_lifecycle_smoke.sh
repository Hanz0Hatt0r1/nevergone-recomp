#!/usr/bin/env bash
set -euo pipefail

PACKAGE='org.nevergone.recomp'
COMPONENT='org.nevergone.recomp/.MainActivity'
APK="$(find android/app/build/outputs/apk/debug -name '*.apk' -print -quit)"

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
      # An unaccelerated emulator can briefly reconnect after reporting boot
      # complete. Require a second healthy round-trip before installation.
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

    # Under software emulation ActivityManager/adb can briefly time out or lose
    # its binder transport while the guest stays booted. Retry only those
    # transport/service failures. Component, permission and application errors
    # remain immediate hard failures.
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

wait_for_android_services
adb install -r "$APK"
adb logcat -c

# Initial foreground launch.
start_activity 'initial launch'
sleep 5
INITIAL_PID="$(require_pid 'initial launch')"
require_foreground 'initial launch'
fatal_check 'initial launch'

# Home pauses the Activity while keeping the process alive. The same PID
# requirement makes this a real pause/resume path rather than a silent restart.
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

# Finish the Activity with Back, then start it again immediately. Requiring the
# same PID proves Activity/GLSurfaceView recreation happened inside the existing
# native process rather than through a process restart.
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

# Cold restart covers complete process/native-runtime initialization after the
# lifecycle/surface path above.
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

adb shell am force-stop "$PACKAGE"
