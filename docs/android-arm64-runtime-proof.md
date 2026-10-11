# Android arm64 runtime evidence

Issue #149 requires runtime evidence on the release architecture. The x86_64 emulator jobs and the Android 15 16 KiB image are useful CI gates, but neither proves that the normal arm64 APK survives the same lifecycle on an arm64 target.

## Run the proof

Build or obtain the normal project APK that contains `lib/arm64-v8a/*.so`, connect an Android 15 or Android 16 arm64 device through `adb`, then run:

```bash
bash tools/android_arm64_runtime_smoke.sh /path/to/nevergone.apk
```

If more than one adb device is connected, set `ANDROID_SERIAL` before running the command. The harness refuses targets outside API 35/36, refuses a primary ABI other than `arm64-v8a`, and refuses an APK without arm64 native libraries.

The wrapper runs the same lifecycle contract used by CI:

1. foreground launch;
2. native renderer counter baseline;
3. Home/pause with the process retained;
4. resume with increased pause/resume counters;
5. Activity/`GLSurfaceView` recreation in the retained process with increased GL surface generation;
6. force-stop and cold restart;
7. Java/native fatal-crash rejection.

The APK is staged through a temporary directory so the shared lifecycle script can run unchanged. Existing build outputs are not overwritten.

## Evidence output

By default the run writes to `artifacts/android-arm64-runtime/`. Override this with `NEVERGONE_EVIDENCE_DIR`.

`runtime-report.txt` records:

- pass/fail status and UTC timestamp;
- Android release and API level;
- manufacturer, model and device codename;
- primary ABI and ABI list;
- runtime page size;
- SHA-256 of the tested APK;
- the retained lifecycle transcript filename.

`lifecycle-smoke.txt` is the sanitized stdout/stderr transcript of the shared lifecycle proof. It preserves the observed renderer `resume events`, `pause events`, and `GL surface generation` values as well as any bounded ActivityManager retry diagnostics. The selected adb serial is replaced with `<adb-serial>` before the transcript is written.

The evidence deliberately omits the adb serial number and build fingerprint.

`logcat-nevergone.txt`, `activity-nevergone.txt`, and `package-nevergone.txt` retain bounded diagnostics useful when the lifecycle proof fails. The logcat capture is filtered to Nevergone, AndroidRuntime and fatal-signal lines rather than dumping unrelated device logs.

## Evidence validation

A successful device run is not accepted solely from the shell exit code. Before reporting success, the harness runs:

```bash
python3 tools/android_runtime_evidence.py artifacts/android-arm64-runtime/runtime-report.txt
```

The validator fails closed unless the report has all required fields, `status=pass`, Android API 35 or 36, primary `arm64-v8a`, an ABI list containing `arm64-v8a`, a supported 4 KiB or 16 KiB page size, a lowercase APK SHA-256 digest, a UTC timestamp, and a local lifecycle transcript containing all four renderer-counter phases. The validator also rejects path traversal in the transcript field and common unsanitized adb serial forms.

For diagnosing an intentionally failed run without treating it as release proof, use `--allow-failure`; schema and transcript checks still apply where present. Synthetic positive and negative cases are exercised by `tools/test_android_runtime_evidence.py` in the lightweight Android runtime workflow.

## Acceptance

Do not mark the arm64 portion of #149 complete merely because this harness exists. Attach or preserve `runtime-report.txt`, `lifecycle-smoke.txt` and the bounded diagnostics from an actual API 35/36 `arm64-v8a` target, then record that API/device/ABI/page-size combination in `docs/android-runtime-validation.md`.
