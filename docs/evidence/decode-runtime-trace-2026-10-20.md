# Runtime check of `cocos2d::Decode` (2026-10-20)

## Source and scope

This evidence is derived from two local JSONL traces of the original Never Gone 1.0.9 ARM32 app on a physical Android device. One was captured during the user's reported play session; the second came from a rebuilt resource-trace APK used for a launch check. The original APKs, raw traces, file contents, and complete asset path inventory are not committed.

The normalized observations are in [`decode-runtime-trace-2026-10-20.json`](decode-runtime-trace-2026-10-20.json). They retain per-call offsets, input lengths, keys, sample counts, and comparison outcomes; native pointers, register snapshots, and file contents are omitted. The raw trace hashes are included there so the summaries can be matched to the local captures.

## Decoder observations

The traces contain 85 calls to `cocos2d::Decode` at module offset `0x534a41`:

- 83 non-empty calls matched `output[i] = ((input[i] XOR key) - (i mod 127)) & 0xff` at every checked byte.
- No non-empty call had a mismatch.
- Two zero-length calls were inconclusive because they had no output bytes to compare.
- The probe checked 10,392 output bytes. Six calls were checked in full at lengths 48, 107, and 113; the other 77 non-empty calls sampled 128 evenly spaced positions.
- The largest input was 2,758,344 bytes, so its runtime check was sampled rather than exhaustive.
- All recorded calls used key `1`.

This supports the existing static and unidbg recovery. The probe itself compares a bounded set of runtime bytes and does not claim that every byte of a large buffer was inspected.

## Resource trace and launch check

The earlier user-reported run lasted 75.102 seconds and contained 42 decoder calls, 46 `getFileData` calls, and 42 `LoadGameLevels` calls. Its file trace stopped at its configured 200-event limit.

The rebuilt resource-trace APK was based on the original 1.0.9 ARM32 package, signed with the already-installed debug certificate, and configured with the `scene` / `resources` profile. Its SHA-256 was `5a9cf1280dd549b0a6a8a11adf6d562b994cdb86daa05329631b9412fd288409`. The trace kept only `LoadGameLevels`, `getFileData`, and `Decode` native hooks; libc file tracing was capped at 3,000 events total and 1,000 per intercepted API.

After launch, the game showed its existing native-library warning for the relative `DT_NEEDED` entry `./obj/local/armeabi-v7a/libffmpeg.so`. Dismissing the warning reached the title screen. Tapping **Tap to start** reached the existing `DAWN Lv.6` save-selection screen. The app process remained alive for the 67.006-second trace window; logcat contained no `FATAL EXCEPTION`, fatal signal, `SIGSEGV`, or `SIGABRT` for the run. No save slot was selected, and the save-file SHA-256 values matched the pre-install values.

The rebuilt trace recorded 2,000 libc file-access events (1,000 `open`, 1,000 `fopen`) and 961 distinct app-relative asset paths. The per-API limits were reached, so later OS-level file accesses are absent. The local path inventory remains outside the repository. Game-level `getFileData`, `LoadGameLevels`, and `Decode` events continued to be recorded.

## Limits

The launch check verifies startup through the save-selection screen only; it does not verify a gameplay session with the rebuilt APK. The resource profile does not instrument enemy or combat behavior. The native-library warning is present on the original package and was non-fatal in this run.
