# BeatenEffect helper field provenance v26

## Why this stage exists

v25 removes the four protobuf `equipToolsListMsg::SharedDtor` receiver failures, leaving ten unresolved receiver classes. Seven are `BeatenEffect::setOpacity` calls. Calls 0..4 already have direct `CCSprite::create*` writes to exact owner fields, but the class-wide alias gate remains open. Calls 5 and 6 additionally lacked a proven non-null writer for `+0x158` and the indexed `+0x14c/+0x150/+0x154` group.

The v24 class inventory exposed two static/helper-looking functions that write values created by `cocos2d::CCSprite::createWithSpriteFrame` through parameters rather than through a decompiler-proven `BeatenEffect *this`:

- `002a3a20` — `BeatenEffect::createBeatenBloodPointEffect02`;
- `002a3c74` — `BeatenEffect::createBeatenBloodPointEffect`.

v26 traces those writes without assuming their destination parameter is `this`.

## Evidence chain

`ExportExactParameterFieldFactoryStore.java` identifies the exact parameter index and constant relative offset used by each helper `STORE`. The plan script selects only a unique destination parameter carrying a known `CCSprite::create*` result.

`ExportCallerArgumentSubtree.java` then enumerates every direct caller and exports a bounded SSA subtree for the actual argument at that parameter index. The v26 audit accepts a field-factory candidate only when all direct callers decompile, every caller contains a matched call, and every actual argument reduces exactly to `BeatenEffect* + constant`. The helper's own store offset is added to the caller offset, producing the final owner-field offset. Dynamic/indexed or ambiguous expressions remain unresolved.

The same run inspects parameter-zero relative stores in the finite whole-owner callee set already observed by the v24 alias inventory:

- `cocos2d::CCNode::removeAllChildrenWithCleanup` (`00526124`);
- `cocos2d::CCLayer::~CCLayer` (`005353f8`);
- `cocos2d::CCLayer::CCLayer` (`00535204`);
- `cocos2d::CCNode::addChild` (`00525dac`);
- `cocos2d::CCCallFunc::create` (`0051f3c8`).

The static vtable catalog must also identify `BeatenEffect` slot `0xbc` exactly as `cocos2d::CCNode::addChild`. v26 reports whether any of those known callees directly writes one of the target receiver-pointer fields `+0x128..+0x158`.

This is still evidence-only. v26 does not promote receiver classes and does not weaken the remaining unknown-owner/callback/indirect-call frontier. A later finalizer may consume a proven field set together with a closed field-specific alias gate.

## Run

```bash
bash tools/ml/run_nevergone_beateneffect_helper_field_provenance_v26.sh
```

If v25 outputs are absent, the runner invokes the v25 runner first. Diagnostics are archived even when a later Ghidra probe fails.

Output bundle:

```text
~/nevergone-beateneffect-helper-provenance-v26.tar.gz
```
