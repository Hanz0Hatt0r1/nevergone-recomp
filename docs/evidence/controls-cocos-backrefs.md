# P1A: Controls Cocos retained back-references

The retained ControlsButton, joystick, scheduler and action-callback channels were already closed by the preceding exact-ELF audits. This note covers the two remaining known Cocos channels: `CCNode` parent pointers created by `addChild`, and CCLayer touch-dispatch retention.

All evidence is pinned to original ARMv7 `libcocos2dcpp.so` SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Parent back-reference

Two recovered Controls paths call inherited `CCNode::addChild`: `Controls::showAToJTip()` adds a `CCSprite`, while `Controls::init()` creates a `CCLayer`, stores it at `Controls+0x178`, and adds it as a child. The child vtables resolve slot `0xc8` to exact `CCNode::setParent(CCNode*)`; its six-byte body stores the parent pointer at `CCNode+0xa8`.

A bounded exact-instruction scan across symbolized `CCNode`, `CCLayer`, and `CCSprite` methods finds exactly 12 immediate word accesses to `+0xa8`: nine loads and three stores. This characterizes the native direct-access surface but does not prove that code reached through a parent pointer cannot later call arbitrary parent methods.

## Touch-dispatch retention

`Controls::init()` enables inherited CCLayer touch handling. Constructor defaults leave targeted mode/script priority fields unchanged, so `CCLayer::registerWithTouchDispatcher()` follows the standard-delegate path. It passes the `CCTouchDelegate` secondary subobject at `Controls+0xe0` to `CCTouchDispatcher::addStandardDelegate`.

`CCTouchHandler::initWithDelegate()` retains that delegate and stores it at `CCTouchHandler+0x14`. The Controls secondary vtable has `this` adjustment `-0xe0`; all eight touch thunk entries resolve to inherited CCLayer touch methods and begin by subtracting `0xe0`.

The four inherited multi-touch callbacks contain no direct writes to `Controls+0x124/+0x12c/+0x130/+0x134/+0x174`. They can, however, enter the Cocos script-engine path through `Controls+0xdc`; script-engine write-through remains a separate unresolved channel.

## Reproduce

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only
bash tools/ml/run_nevergone_controls_cocos_backrefs.sh \
  /path/to/com.hippiegame.nevergone.apk
```

Output is written under `$HOME/.cache/nevergone-recomp/receiver-provenance/` as `controls-cocos-backrefs.jsonl` and `controls-cocos-backrefs.txt`.

## Conservative conclusion

The two known Cocos retention mechanisms are now structurally characterized: child parent pointer at `+0xa8`, and retained touch delegate `Controls+0xe0 -> CCTouchHandler+0x14`. The remaining hazards are narrowed to parent-mediated call/write-through and script-engine write-through.

Safety remains `whole_program_alias_writes_excluded=false`, `receiver_class_proven=false`, `affects_routing=false`. No production routing/runtime code is changed.
