#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEMP_ROOT="$(mktemp -d)"
cleanup() {
  rm -rf "$TEMP_ROOT"
}
trap cleanup EXIT

mkdir -p "$TEMP_ROOT/android/app/build/outputs/apk/debug" "$TEMP_ROOT/bin"
cat >"$TEMP_ROOT/bin/adb" <<'ADB'
#!/usr/bin/env bash
set -euo pipefail

case "$*" in
  'shell getprop ro.build.version.sdk')
    echo '35'
    ;;
  'shell getprop ro.product.cpu.abi')
    echo 'x86_64'
    ;;
  'shell getconf PAGE_SIZE')
    echo '4096'
    ;;
  'devices -l')
    printf 'List of devices attached\nemulator-test device product:test model:test device:test transport_id:1\n'
    ;;
  'shell dumpsys activity activities')
    cat <<'EOF'
noise-before
mResumedActivity: ActivityRecord{test org.nevergone.recomp/.MainActivity}
noise-after
EOF
    ;;
  'logcat -d -t 400')
    cat <<'EOF'
I/OtherApp: SECRET-LOGCAT-NOISE
E/AndroidRuntime: FATAL EXCEPTION: main
E/AndroidRuntime: Process: org.nevergone.recomp, PID: 123
EOF
    ;;
  'shell dumpsys package org.nevergone.recomp')
    cat <<'EOF'
versionCode=1 minSdk=23 targetSdk=36
versionName=test
SECRET-PACKAGE-NOISE
primaryCpuAbi=x86_64
EOF
    ;;
  shell\ rm\ -f*)
    ;;
  *)
    echo "unexpected adb invocation: $*" >&2
    exit 2
    ;;
esac
ADB
chmod +x "$TEMP_ROOT/bin/adb"

set +e
OUTPUT="$(
  cd "$TEMP_ROOT"
  PATH="$TEMP_ROOT/bin:$PATH" bash "$REPO_ROOT/tools/android_emulator_lifecycle_smoke.sh" 2>&1
)"
STATUS=$?
set -e

if [[ $STATUS -ne 1 ]]; then
  echo "Expected missing-APK failure status 1, got ${STATUS}" >&2
  printf '%s\n' "$OUTPUT" >&2
  exit 1
fi

for expected in \
  'Never Gone Recomp debug APK was not found' \
  '--- Never Gone Recomp lifecycle failure diagnostics ---' \
  'android_sdk=35' \
  'primary_abi=x86_64' \
  'page_size=4096' \
  'mResumedActivity: ActivityRecord{test org.nevergone.recomp/.MainActivity}' \
  'E/AndroidRuntime: FATAL EXCEPTION: main' \
  'Process: org.nevergone.recomp, PID: 123' \
  'versionName=test' \
  'primaryCpuAbi=x86_64' \
  '--- end lifecycle failure diagnostics ---'; do
  if ! grep -Fq -- "$expected" <<<"$OUTPUT"; then
    echo "Missing expected failure diagnostic: $expected" >&2
    printf '%s\n' "$OUTPUT" >&2
    exit 1
  fi
done

if grep -Fq 'SECRET-LOGCAT-NOISE' <<<"$OUTPUT"; then
  echo 'Unrelated logcat line leaked into bounded diagnostics' >&2
  exit 1
fi
if grep -Fq 'SECRET-PACKAGE-NOISE' <<<"$OUTPUT"; then
  echo 'Unrelated package line leaked into bounded diagnostics' >&2
  exit 1
fi

echo 'Android lifecycle failure diagnostics smoke: ok'
