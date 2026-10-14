# P1A: exact-native closure of the 14 external full-Controls* sites

The earlier Ghidra planner isolated 14 external first-hop calls receiving the unadjusted `Controls*`: 11 direct calls and 3 virtual calls. Later exact-native audits independently closed the callback/retention mechanisms, so this stage reconciles the entire frontier directly against the original APK instead of requiring another local Ghidra export.

## 11 direct calls

The source BL instructions are decoded and matched to exact ELF symbols.

- `CCNode::scheduleOnce` x2 and `CCNode::schedule` x1 are covered by the retained scheduler callback closure (`comboEnd`, `tick`).
- `CCCallFuncN::create` x1 is covered by the action callback closure (`showKillTipsCallback`).
- `ControlsButton::create` x3 is covered by the exact `ControlsButton+0x110/+0x114/+0x118` callback retention closure.
- `CCNode::unscheduleAllSelectors` x1 tail-calls `CCScheduler::unscheduleAllForTarget`. Exact function bodies are SHA pinned; the Controls pointer is preserved as the scheduler target argument and the scheduler's immediate write is internal scheduler state `+0x30`, not a Controls field.
- `CCLayer` constructor x1 writes only inherited/base offsets through `+0x100`, none of the recovered Controls pointer fields.
- `CCLayer` destructor x1 is SHA pinned and has no direct write to `Controls+0x124/+0x12c/+0x130/+0x134/+0x174`.
- `operator delete` x1 occurs only after `ControlsD2` in the deleting destructor and is terminal for object lifetime.

## 3 virtual calls

The source BLX instructions and exact Controls vtable relocations resolve:

- `0x2979e6`, slot `0xb8` -> `CCNode::addChild`;
- `0x297e74`, slot `0x1ac` -> `CCLayer::setTouchEnabled`;
- `0x297ef2`, slot `0xb8` -> `CCNode::addChild`.

Those paths are rechecked through the merged parent/touch audits: child parent retention is at `CCNode+0xa8`, standard touch delegate retention is `Controls+0xe0 -> CCTouchHandler+0x14`, parent targets have no dangerous direct native writes, and no packaged static/dlsym route installs a Cocos script engine.

## Result

All **14/14 known external full-this first-hop sites** are now statically classified against the exact original APK. None of the known frontier targets directly writes the recovered receiver-pointer fields.

This is not a whole-program memory-safety theorem. Injected/hooked code, corruption, debugger mutation, or a previously undiscovered alias channel remain outside the proof boundary. Consequently `whole_program_alias_writes_excluded=false`, `receiver_class_proven=false`, `affects_routing=false`.

Reproduce:

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only
bash tools/ml/run_nevergone_controls_full_this_static_closure.sh \
  /path/to/com.hippiegame.nevergone.apk
```
