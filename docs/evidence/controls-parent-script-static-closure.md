# P1A: Controls parent-mediated writes and script-engine static reachability

This follows the retained parent/touch characterization merged in PR #587. The exact original APK is required because the script-engine check includes both ARM ELF and DEX/native-library inventory.

## Parent-mediated paths

The exact Controls vtable resolves the parent-consumer slots observed in `CCNode`/`CCSprite` code as follows:

| Slot | Controls target |
| --- | --- |
| `0xcc` | `CCNode::getParent()` |
| `0xdc` | `CCNode::removeChild(CCNode*, bool)` |
| `0xf0` | `CCNode::reorderChild(CCNode*, int)` |
| `0x160` | `CCNode::nodeToParentTransform()` |
| `0x210` | inherited `CCLayer::ccTouchesEnded` thunk |

The first four targets and the direct `CCNode::transform()` path are exact-function SHA pinned. Their complete immediate `STR.W/STRB.W` inventory contains only generic CCNode offsets `+0xcd/+0xcf/+0xd2`; none writes the Controls pointer fields `+0x124/+0x12c/+0x130/+0x134/+0x174`.

The unusual `0x210` path collapses to the already identified inherited touch/script path rather than a new native field-writer class. Therefore the known native parent-mediated calls do not directly mutate the recovered Controls receiver-pointer fields.

## Packaged script-engine assignment

`CCNode` construction captures `CCScriptEngineManager::engine` into `CCNode+0xdc`; the singleton initializes its sole engine pointer to null. The only setter symbol is `CCScriptEngineManager::setScriptEngine(CCScriptEngineProtocol*)`.

Across the exact packaged APK:

- there are **zero** decoded Thumb `BL`/unconditional `B.W` branches to `setScriptEngine` in the complete `.text` section;
- there are **zero** 32-bit PROGBITS function-pointer literals equal to either the ARM address or Thumb address of that setter;
- `classes.dex` contains no `setScriptEngine`, `CCScriptEngineManager`, or mangled setter reference;
- the APK contains only `libcocos2dcpp.so` and `libffmpeg.so` under `lib/armeabi-v7a/`;
- no `CCLuaEngine` implementation is linked, while the game's separate `lua_system::initLua/uninitLua` bodies are SHA pinned.

This means the packaged static application has no discovered path that installs a Cocos `CCScriptEngineProtocol` implementation, so the inherited touch callback's `Controls+0xdc` gate is not statically reachable as a write-through channel.

This is deliberately not phrased as a proof against `dlopen`/`dlsym`, injected code, hooking, memory corruption, or other runtime modification outside the packaged static program. Accordingly `whole_program_alias_writes_excluded=false` and no receiver-routing promotion is authorized by this audit alone.

## Reproduce

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only
bash tools/ml/run_nevergone_controls_parent_script_static_closure.sh \
  /path/to/com.hippiegame.nevergone.apk
```

Output: `$HOME/.cache/nevergone-recomp/receiver-provenance/controls-parent-script-static-closure.jsonl`.

Safety: `receiver_class_proven=false`, `affects_routing=false`.
