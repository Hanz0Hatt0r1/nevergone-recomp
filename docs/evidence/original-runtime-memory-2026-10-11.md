# Original Android runtime memory evidence (2026-10-11)

This note records **derived observations only** from the original Never Gone 1.0.9 ARMv7 process on a Redmi Note 8 Pro running Android 13. The APK, memory pages, screenshots, process maps, and full manifests remain local and are not part of this repository. The static ELF used for the Ghidra comparison has SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Full process capture: title screen

The original process (PID 29915) was on the **"Tap to start" title screen**, as checked against the local screenshot, when `tools/android_dump_original_process_memory.py --full` captured it. The completed manifest reports:

| Result | Count | Bytes |
| --- | ---: | ---: |
| Readable mappings captured | 2,418 | 1,278,676,992 |
| Readable mappings that `/proc/PID/mem` could not supply | 23 | 40,763,392 |

The 23 failures were six `/dmabuf` mappings, thirteen `/dev/mali0` mappings, three binder mappings, and one deleted AudioFlinger ashmem mapping. They are device-backed mappings; their absence means this is **not a byte-complete process snapshot**. The captured set includes 296 `[anon:libc_malloc]` mappings (17,137,664 bytes), as well as other readable anonymous, stack, file and library mappings. Each successful mapping was length-checked and SHA-256 hashed in the local manifest. That manifest's SHA-256 is `fe6bdc2c296a30f07648d9b197359b5b0bd649d10f2b2c987eb8f1dca092c0b8`.

This run establishes that the full-dump mode can capture the readable game heap, but it does **not** observe the player animation in gameplay. The process stayed live during the sequential read, so mappings were sampled over time rather than atomically.

## Focused gameplay capture: action-system fields

A separate earlier capture of the original process (PID 25803) was taken while a gameplay scene with Dawn, dialogue and HUD was visible. It retained five native library mappings (9,891,840 bytes) locally. A scan of 680 `[anon:libc_malloc]` regions (68,562,944 bytes) for relocated vtable address points found 66 `MeActionsSystem` and 11 `EnemyActionsSystem` pointer matches. The complete derived hit list is [`original-runtime-vptr-hits-2026-10-11.json`](original-runtime-vptr-hits-2026-10-11.json). The runtime address points were `0xbb548768` for `MeActionsSystem` and `0xbb53d418` for `EnemyActionsSystem`, obtained from the identified ELF vtable symbols plus the `0xbac58000` load base and the Itanium vtable address-point adjustment. These are **candidates**, because an allocator slot can retain a stale pointer after an object has ceased to be live.

The bounded field samples in [`original-runtime-frame-samples-2026-10-11.tsv`](original-runtime-frame-samples-2026-10-11.tsv) preserve the observed values without copying memory bytes. One `MeActionsSystem` candidate at `0xb58f3000` had `+0x1a4` values `10, 2, 8, 1` across four samples; multiple `EnemyActionsSystem` candidates changed at `+0x294` across three samples. These changes support treating both fields as runtime frame counters. They do **not** prove that the sampled `MeActionsSystem` candidate owns the first player `action28`, or that an enemy candidate describes the player.

The independently established *initial* player frame remains the instruction-path evidence in [`me-actions-initial-frame.md`](me-actions-initial-frame.md) and [`player-current-frame-stores.tsv`](player-current-frame-stores.tsv): `MeActionsSystem::play()` writes zero to its own `+0x1a4` before selecting the first primary frame. The gameplay memory samples are corroborating dynamic evidence, not the source of that initial value.

## Reproduction

With the original game running on a connected device whose `adbd` supports `adb root`:

```bash
python3 tools/android_dump_original_process_memory.py \
  --full --output /private/local/path/original-memory-YYYYMMDD-HHMMSS
```

Omit `--full` to retain only the three `libcocos2dcpp.so` mappings and writable `libGLES_mali.so`/`libEGL.so` mappings. Each invocation requires a new empty output directory. The tool restores non-root `adbd` after capture. Keep its output private: raw process memory may contain original game assets and unrelated device or account data.
