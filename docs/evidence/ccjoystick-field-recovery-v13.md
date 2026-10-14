# CCJoyStick field receiver recovery v13

Evidence-only production recovery for the one non-quarantined `receiver_not_single_pointer` row remaining after v12:

- source: `libcocos2dcpp.so:002a647e`
- call: `4`
- slot: `0x58`

The accepted proof chain is deliberately narrow:

1. `ParameterRelativeReceiverFieldAudit/2` proves the call is in `CCJoyStick::setStickTexture` and the receiver is `LOAD(this + 0x104)`.
2. `GhidraExactParameterFieldFactoryStore/1` proves exact `CCJoyStick::init` stores a direct factory-call result to `this + 0x104`.
3. `CallRootFactoryReturnAudit/1` proves the non-null factory return is constructor-proven `cocos2d::CCSprite` with unique vtable `libcocos2dcpp.so:009363d8`.
4. `CCJoyStickFieldFactoryProvenanceAudit/1` joins those facts and checks `CCSprite` slot `0x58` in `StaticVtableSlotCatalog/3`, yielding a single target `libcocos2dcpp.so:00525d18` (`cocos2d::CCNode::getPosition`).
5. The v13 finalizer independently re-checks class, vtable, slot and target against the catalog before emitting a normal exact-vtable query.

The v13 runner always regenerates v12 first, preserving the quarantine of `libcocos2dcpp.so:002a7594` calls `3`, `4`, and `7`. No field-offset-only, owner-name-only, sibling, neighboring-object or cross-vtable inference is permitted.

Expected real-data effect relative to current v12:

- queries: `32 -> 33`
- unresolved virtual rows: `28 -> 27`
- `receiver_not_single_pointer`: `4 -> 3`
- the three remaining rows are exactly the quarantined `002a7594` calls `3/4/7`

`affects routing: no` remains the evidence-generation contract; production resolution changes only after all v13 gates pass.
