# Residual receiver owner-alias flow v23

## Scope

v20 corrected an unsafe receiver-class transfer: the type of an owner parameter must not be reused for a virtual receiver loaded from one of that owner's fields. v21 then enumerated exact parameter-relative field writes, while v22 added a conservative class-wide alias inventory and structural field maps for the newly exposed residual receivers.

The real v22 evidence showed that the first alias inventory was intentionally too conservative for final recovery: it treated any expression derived from parameter zero as a potential pointer-slot alias. In particular, passing `LOAD(this + field)` as a call argument was grouped with passing `this` or `&this->field`, even though the loaded pointee value cannot by itself replace the pointer stored in the owner field.

v23 refines that distinction without weakening fail-closed handling of actual alias channels.

## Exported flow classes

`ExportClassOwnerAliasFlow.java` decompiles every Ghidra function owned by an exact C++ class name and first verifies whether high-P-code parameter zero is actually a pointer to that class. Static/helper functions whose parameter zero is unrelated are not interpreted as owner methods.

For owner-proven methods each STORE address and each CALL/CALLIND argument is classified as one of:

- `owner_address`: the expression is exactly `this + constant`; offset zero represents the whole owner, non-zero represents an address into the owner;
- `owner_field_value`: the expression is a value loaded from an exact `this + constant` field, possibly through transparent or simple arithmetic nodes;
- `owner_derived_unknown`: parameter zero participates in the expression but an exact address/value interpretation is not proven;
- `unrelated`: no owner flow is present;
- `owner_parameter_unproven`: parameter zero is not independently proven to be the class owner.

Passing an `owner_field_value` to a method does **not** prove an alias to the pointer slot and therefore does not by itself block field stability. Passing the whole owner, passing an exact target-field address, unresolved owner-derived call arguments, and unresolved owner-derived STORE addresses remain explicit blockers.

## Real v22 frontier carried into v23

The corrected v20 inference contains 14 unresolved virtual receivers.

`BeatenEffect::setOpacity` has seven unresolved calls at `libcocos2dcpp.so:002a47c6`. Structural receiver slices map calls 0..4 exactly to fields `+0x130`, `+0x134`, `+0x138`, `+0x12c`, and `+0x128`. v21 records non-null writes to those fields only from `cocos2d::CCSprite::create*`. Call 5 maps to `+0x158`, for which no non-null factory write is currently proven. Call 6 spans the indexed `+0x14c/+0x150/+0x154` group, likewise without a proven non-null factory assignment.

`CCJoyStick::setDockTexture` at `libcocos2dcpp.so:002a651c` contains two unresolved calls. Both load receiver field `this+0x10c`; v21 independently records `CCJoyStick::init` storing a `cocos2d::CCSprite::create` return into `+0x10c`.

`PVEDATA::equipToolsListMsg::SharedDtor` at `libcocos2dcpp.so:00299d90` contains four unresolved slot-`0x4` calls whose receivers load exact fields `+0x8`, `+0xc`, `+0x10`, and `+0x14`. The recovered protobuf schema defines exactly four `equipToolsDataMsg` members, but v23 treats this only as an expected semantic correspondence. It additionally exports native parameter-field writes for `PVEDATA::equipToolsListMsg`; schema information alone never promotes a receiver class.

`CCJoyStick::timerUpdate` at `libcocos2dcpp.so:002a5dd0` still reads `this+0x14c`; no non-null assignment proving the pointee class has been observed, so this case remains fail-closed regardless of alias results.

## Candidate join

`audit_nevergone_residual_receiver_candidates_v23.py` joins exact structural field mappings, v21/v23 native field writes, and the v23 pointer-slot alias audit. A candidate is marked `receiver_class_proven=true` only if all required native evidence agrees and the relevant owner alias frontier is closed.

The synthetic smoke demonstrates the maximum currently eligible set under a closed alias frontier: five `BeatenEffect` calls, two `CCJoyStick::setDockTexture` calls and four protobuf `SharedDtor` calls. This is a test of gate behavior, not a claim that the real binary will prove all eleven; the real v23 run determines that.

No v23 artifact changes production routing. Every record is evidence-only and carries `affects_routing=false`.

## Run

```bash
bash tools/ml/run_nevergone_owner_alias_flow_v23.sh
```

The diagnostic bundle is written to:

```text
~/nevergone-owner-alias-flow-v23.tar.gz
```
