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
      # The unaccelerated API 35 emulator can briefly reconnect after reporting
      # boot complete. Require a second healthy round-trip before installation.
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
  if ! adb shell dumpsys activity activities | grep -Eq \
      'mResumedActivity.*org\.nevergone\.recomp|topResumedActivity=.*org\.nevergone\.recomp'; then
    echo "Never Gone Recomp is not resumed during ${phase}" >&2
    adb shell dumpsys activity activities >&2
    exit 1
  fi
}

wait_for_android_services
adb install -r "$APK"
adb logcat -c

# Initial foreground launch.
adb shell am start -W -n "$COMPONENT"
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

adb shell am start -W -n "$COMPONENT"
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
adb shell am start -W -n "$COMPONENT"
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
adb shell am start -W -n "$COMPONENT"
sleep 3
require_pid 'cold restart' >/dev/null
require_foreground 'cold restart'
fatal_check 'cold restart'

adb shell am force-stop "$PACKAGE"
