# P1A: 15 Controls alias operand roots cross-checked against original ARM ELF

## Source and boundary

The user-returned `nevergone-controls-alias-operand-roots.tar.gz` passed
its original Ghidra auditor: **15/15 sites**, no errors. The follow-up
`tools/ml/audit_nevergone_controls_alias_native_crosscheck.py` pins the
original `libcocos2dcpp.so` SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`,
checks the archive's plan/roots/audit identities and compares original
Thumb instructions, ELF dynamic symbols, Controls vtable bytes and
`R_ARM_RELATIVE` relocations. No original binary is committed.

## Nine indirect STORE bases

All nine Ghidra `STORE` destination expressions use
`LOAD(Controls + field) + child_offset`; their original ARM instructions
agree with the Ghidra site addresses. The original ELF witnesses are
eight `strb.w` (one-byte) stores and one `str.w` (four-byte) store.

| Controls pointer field | Child object offset | Count | Original instruction |
| --- | --- | ---: | --- |
| `+0x124` | `+0x150` | 5 | `strb.w` |
| `+0x12c/+0x130/+0x134` | `+0x11d` | 3 | `strb.w` |
| `+0x124` | `+0x14c` | 1 | `str.w` |

The one pointer-width store at Ghidra `002a808c` is
`STR.W (Controls + 0x104) -> *( *(Controls + 0x124) + 0x14c )`,
independently confirmed by original Thumb `add.w r2,r4,#0x104` and
`str.w r2,[r3,#0x14c]`. This is a back-reference into the Controls
object, **not** an assignment to `Controls+0x124` itself. The
construction-linked candidate for `Controls+0x124` is `CCJoyStick`;
the `+0x12c/+0x130/+0x134` factory candidates are `ControlsButton`.
These are not unique runtime class proofs.

## Three CALLIND targets on Controls this

The Ghidra receiver root and call argument are both parameter zero;
the nested vtable load is `LOAD(Controls+0x0)`. The original
`_ZTV8Controls` ELF object (virtual address `0x8e4490`, 592 bytes,
vptr address point `+8`) has `R_ARM_RELATIVE` entries for both
slots. Exact Thumb `BLX` instructions and ELF dynamic function
symbols agree:

| Ghidra call | Vtable slot | Original ELF target | Ghidra target | Symbol |
| --- | --- | --- | --- | --- |
| `002a79e6` | `0xb8` | `00515dbc` | `00525dbc` | `cocos2d::CCNode::addChild` |
| `002a7e74` | `0x1ac` | `00524da8` | `00534da8` | `cocos2d::CCLayer::setTouchEnabled` |
| `002a7ef2` | `0xb8` | `00515dbc` | `00525dbc` | `cocos2d::CCNode::addChild` |

These are **Controls-vtable target candidates**. A subclass or
runtime vptr mutation could change the actual target. No dispatch
routing was modified.

## Three ControlsButton factory callbacks

The three `Controls::init` calls at `002a8118`, `002a816a` and
`002a81bc` call the exact ELF
`ControlsButton::create(char const*,char const*,cocos2d::CCObject*,...)`
symbol. In each, `call_arg_2` is the unadjusted
`Controls*` parameter zero. This proves the pointer is **passed as
a callback object argument**; whether and how the factory retains
or writes through that pointer is not proven.

## Reproduce locally

```bash
cd `/nevergone-recomp
git switch main
git pull --ff-only
python3 tools/ml/audit_nevergone_controls_alias_native_crosscheck.py \
  `/nevergone-controls-alias-operand-roots.tar.gz \
  /path/to/com.hippiegame.nevergone.apk \
  --details-output /tmp/nevergone-controls-alias-native-crosscheck.jsonl
```

## All 37 register-rooted STOREs: exact original Thumb witnesses

A second offline auditor,
`tools/ml/audit_nevergone_controls_register_store_native.py`,
consumes the original triage JSONL from the same archive and pins
**all 37** previously register-rooted unknown STORE sites to original
Thumb `STR.W` or `STRB.W` immediate instructions. All five relevant
`Controls` method prefixes are SHA-256-pinned; original Thumb
`MOV r4,r0` assignments and the absence of intervening `r4`
redefinitions before the checked STOREs were independently inspected.
`r0`-based stores are limited to instruction windows before the
entry `this` register is reused.

| Native Controls field STORE witness | Count |
| --- | ---: |
| `STR.W` (4-byte) | **24** |
| `STRB.W` (1-byte) | **13** |
| Base register `r4` (saved `Controls* this`) | 28 |
| Base register `r0` (entry `Controls* this`) | 9 |
| Writes to `Controls+0x124/0x12c/0x130/0x134` | **0** |

These 37 writes are to direct `Controls` fields, not to child
objects. Together with the nine LOAD-based child-object STOREs above,
all **46/46** originally unknown local STORE sites in the 38-method
scan are classified by original native instructions.

Run the second auditor:

```bash
python3 tools/ml/audit_nevergone_controls_register_store_native.py \
  ~/nevergone-controls-alias-operand-roots.tar.gz \
  /path/to/com.hippiegame.nevergone.apk \
  --details-output /tmp/nevergone-controls-register-store-native-audit.jsonl
```

**Interprocedural aliases remain open.** In particular, the three
`ControlsButton::create` calls receive `Controls* this` as callback
object, and the joystick back-reference points into the owner.
Neither local STORE inventory excludes writes from other functions.

A bounded structural result is not a
whole-program write exclusivity proof. All output retains
`all_alias_writers_excluded=false`,
`dynamic_receiver_class_proven=false`, `affects_routing=false`.
