# P1A: Controls auxiliary receiver fields v16

v16 handles only the three remaining old slot-`0x80` candidates whose owner is
`Controls` rather than `ControlsButton`:

| Source/call | Actual receiver field | Factory class | Slot `0x80` target |
| --- | --- | --- | --- |
| `002a74c0` call 3 (`002a752a`) | `Controls+0x124` | `CCJoyStick` | `00525d2e` |
| `002a7696` call 0 (`002a76b6`) | `Controls+0x124` | `CCJoyStick` | `00525d2e` |
| `002a7696` call 1 (`002a76c4`) | `Controls+0x174` | `cocos2d::CCSprite` | `00549f1e` |

## Why these fields can now be promoted

The old 38-method Controls Ghidra inventory contains exactly one direct
`this+0x124` STORE (`002a8066`) and one direct `this+0x174` STORE
(`002a80e0`), both in `Controls::init`. The slot80 factory-field witness ties
those stores to `CCJoyStick::create` and `CCSprite::create` respectively.

The gated v14/v15 packaged-program closure additionally establishes:

- all 46 formerly unknown local Controls STOREs are classified by exact Thumb;
- all 35 external unadjusted `Controls*` calls are classified;
- the independent 14/14 full-this first-hop frontier is closed;
- that frontier explicitly treats `0x124` and `0x174` as dangerous owner
  offsets and reports no known target writing them;
- retained button/joystick aliases, scheduler/action callbacks, parent/touch
  backrefs, packaged script-engine reachability and dlsym routes are closed.

`audit_nevergone_controls_aux_field_stability.py` joins the 38-method archive,
the gated v14 stability artifact and the current factory-field candidates. It
emits `NEVERGONE.ControlsAuxFieldStability/1` only when both owner fields have
exactly one direct writer and every packaged alias gate remains valid.

## v16 finalizer

`finalize_nevergone_controls_aux_field_recovery_v16.py` starts from v15 and
requires the three rows above to remain `unresolved_receiver_class`. It checks
exact call/field/class/vtable/target identity and current unique catalog rows:

- `CCJoyStick`, vtable `libcocos2dcpp.so:008f4228`, slot `0x80` ->
  `libcocos2dcpp.so:00525d2e`;
- `cocos2d::CCSprite`, vtable `libcocos2dcpp.so:009363d8`, slot `0x80` ->
  `libcocos2dcpp.so:00549f1e`.

A valid baseline adds exactly **three** exact-vtable queries. The runner checks
that every non-target v15 inference row is unchanged.

The three `ControlsButton+0x104/+0x108/+0x10c` sprite candidates are **not**
covered here; they require a separate complete ControlsButton owner-field
stability audit.

Proof scope remains `exact_packaged_apk_normal_code_execution`.
`whole_program_alias_writes_excluded=false` remains explicit.

## Run

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only

bash tools/ml/run_nevergone_controls_aux_field_recovery_v16.sh \
  /path/to/com.hippiegame.nevergone.apk \
  ~/nevergone-controls-field-alias-evidence.tar.gz \
  ~/nevergone-controls-alias-operand-roots.tar.gz
```

Output: `~/nevergone-controls-aux-field-recovery-v16.tar.gz`.
