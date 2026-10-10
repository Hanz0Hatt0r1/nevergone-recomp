# Native observations: 2026-10-10

## Scope and provenance

Loader repair and bounded dynamic evidence for asset decoding / Cocos2d-x hit testing. Repository base: `55f2abe`. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

[Harness and reproduction](../../../tools/unidbg/README.md). [Run metadata](run.json), [original ELF inventory](libraries.json), [device dependency hashes](dependencies.json), [loader output](loader.log), [synthetic native outputs](native-probes.jsonl).

All five source libraries are ELF32 little-endian ARM EABI5 (armeabi-v7a). Their hashes were checked against the preexisting inventory after experiments: unchanged. Original binaries and assets are not part of this evidence. Device serial, build fingerprint and host paths are omitted.

## Fixes and observed loading

- Standard DT_RELR was not handled by the tested unidbg version. Applied **4,960** relative relocations in emulator memory, including 2,059 in libEGL. Source hashes and all target values are checked before each module's changes.
- Wrapped nested dependency resolution to honor the selected runtime. Previously SDK resource dependencies could silently supply the wrong libc++.
- **48 modules**, **0 unresolved symbols**, explicit **JNI_OnLoad completed**. Constructors remain disabled. This does not prove complete Android initialization.
- Live MCP `check_connection`, `find_symbol`, `read_memory` and `call_symbol` succeeded; `avcodec_version()` returned `0x394164`. See [connection](mcp-connection.json) and [requests/results](mcp-checks.json). Breakpoint and trace workflows are available through the debugger but were not exercised in this experiment.

## Native function evidence

Stage 2 adds [GameSaveData Encode/Decode dynamic comparisons, a local call graph, EnemyActionsData dependency analysis and a Lua binding sample](reconstruction-stage-2.md). Its 36 structured native results are in [save-probes.jsonl](save-probes.jsonl).

| Function | ELF symbol value (Thumb) | Instruction offset | Experiment |
|---|---:|---:|---|
| cocos2d::Decode | 0x534a41 | 0x534a40 | 100 synthetic cases |
| CCRect::containsPoint | 0x517a3b | 0x517a3a | 6 points |
| CCRect::intersectsRect | 0x517aaf | 0x517aae | 6 rectangles |
| JNI_OnLoad | 0x284699 | 0x284698 | explicit loader call |

Observed libcocos2dcpp base: `0x12000000`; size `0x968000`. All 48 runtime bases are in loader.log; addresses are specific to this run.

### Decoder: high confidence for tested inputs

Lengths 0, 1, 2, 126, 127, 128, 253, 254, 255, 1024; keys 0, 1, 127, 255, 0x123; separate buffers and in-place decoding. Input byte i = `(i*73+19)&255`. Every output matched the existing `tools/asset_decoder.py::decode_bytes`; every trailing guard stayed 0x5a. This independently validates the project's XOR/subtract transform and counter wrap at 127 on these cases. No decoder change was needed.

This does not establish behavior for negative lengths, invalid pointers, partial overlaps, or every unsigned key.

### Geometry: high confidence for tested objects

Synthetic float32 rectangle x=10, y=20, width=30, height=40. Both minimum and maximum corner points are included. Points just outside are rejected; the tested NaN x coordinate is rejected. Edge-touching rectangles intersect; rectangles just beyond do not. A zero-size rectangle located inside intersects. Native results are preserved individually in JSONL. These observations can guide compatible UI hit testing and collision boundary tests; they do not validate arbitrary malformed/negative-size rectangles.

## Remaining boundary: EGL

After fixing RELR, `eglGetError` advances past the previous raw-pointer read but faults on a null indirect call: PC=0, LR=`libEGL.so+0x1365f`. See [failed experiment](egl-failure.txt). Its returned register value is not a valid EGL result. Missing initialized dispatch/runtime state is a hypothesis; graphics, constructors and game startup are not validated. Keep further native probes isolated and establish each function's required state first.

## Static candidates for continued reconstruction

- [jni.tsv](jni.tsv): 22 JNI exports, including renderer lifecycle and JNI_OnLoad.
- [lua.tsv](lua.tsv): 596 matching Lua/script-related exported functions.
- [game.tsv](game.tsv): 1,175 matching game functions, including EnemyActionsData, GameScene, GameSaveData and AppDelegate.
- [cocos.tsv](cocos.tsv): 160 matching director/application/file/view/script functions.

Counts describe selected exported symbol rows (aliases included), not unique semantic functions. `inventory.py` can regenerate all exported/imported symbol tables locally. `libraries.json` records DT_NEEDED, initialization arrays and symbol counts. For example EnemyActionsData::loadWBGFile is at instruction offset `0x28f430`; its file/string/object dependencies must be reconstructed before calling it. No game-state parser behavior is inferred from a symbol name alone.

## Verification

From repository root:

```sh
python3 -m unittest discover -s tools/unidbg -p 'test_*.py'
python3 tools/unidbg/verify_evidence.py docs/evidence/unidbg-2026-10-10/native-probes.jsonl
```

Observed: four RELR decoding tests passed; 100 decoder comparisons, 100 guards and 12 geometry observations passed. The same probe passed when run from the source-only harness copied into this branch. GitHub publication is pending authentication; these are local evidence files.
