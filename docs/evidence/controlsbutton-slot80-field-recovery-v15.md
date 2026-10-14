# P1A: ControlsButton slot-0x80 field recovery v15

## Scope

v15 promotes only the eleven old slot-`0x80` calls whose actual receiver
slices load one of the three `Controls` fields already proven stable by the
gated v14 packaged-program closure:

| Source | Calls | Fields |
| --- | --- | --- |
| `libcocos2dcpp.so:002a74c0` | 0, 1, 2 | `+0x12c/+0x130/+0x134` |
| `libcocos2dcpp.so:002a7594` | 0, 1, 2, 5, 6 | `+0x12c/+0x130/+0x134/+0x12c/+0x134` |
| `libcocos2dcpp.so:002a78cc` | 0, 1, 2 | `+0x12c/+0x130/+0x134` |

These eleven rows remained `unresolved_receiver_class` after v14. They are
separate from the three formerly quarantined calls 3/4/7 in `002a7594`, which
v14 already resolves from the dedicated mixed-receiver evidence.

## Evidence join

`run_nevergone_controlsbutton_slot80_field_recovery_v15.sh` first regenerates
the gated v14 baseline, including:

- exact packaged field stability for `Controls+0x12c/+0x130/+0x134`;
- the independent **14/14 external full-Controls-this frontier** gate;
- the three v14 mixed-receiver recoveries.

It then reruns the existing 17-call structural slot-`0x80` Ghidra probe and
`audit_nevergone_slot80_factory_fields.py`. The factory-field auditor confirms
for every selected v15 call:

1. canonical actual CALLIND receiver shape from the structural Ghidra slice;
2. exact `Controls` field offset;
3. pinned original-Thumb `ControlsButton::create` return STORE into that field;
4. candidate vtable `libcocos2dcpp.so:008f46f8`;
5. slot `0x80` target `libcocos2dcpp.so:00525d2e`
   (`cocos2d::CCNode::setVisible(bool)`).

The factory-field artifact deliberately remains a candidate
(`field_writes_exhaustive=false`, `receiver_class_proven=false`). v15 upgrades
only the eleven `ControlsButton` rows because the independent gated v14 proof
establishes packaged-program stability of exactly those three owner fields.

## Deliberately not promoted

Six rows from the 17-call probe remain outside v15:

- 2 `CCJoyStick` candidates from `Controls+0x124`;
- 1 `cocos2d::CCSprite` candidate from `Controls+0x174`;
- 3 `cocos2d::CCSprite` candidates from `ControlsButton+0x104/+0x108/+0x10c`.

Their factory construction evidence is positive, but v14's packaged stability
proof does not cover those owner fields. v15 fail-closes if any of these
classes/fields are substituted for a selected ControlsButton row.

## Finalizer contract

`finalize_nevergone_controlsbutton_slot80_field_recovery_v15.py` requires:

- a valid gated `ControlsButtonPackagedFieldStability/1` summary with the
  14/14 full-this frontier closed;
- all three v14 stability detail rows for `+0x12c/+0x130/+0x134`;
- exactly the eleven source/call/address/field identities above;
- exact per-field original factory STORE witnesses (`0x298120`, `0x298172`,
  `0x2981c2`);
- v14 input status still `unresolved_receiver_class` for every selected row;
- unique current catalog row for `ControlsButton`, vtable `008f46f8`, slot
  `0x80`, single target `00525d2e`, and no runtime mismatch.

A successful baseline adds **exactly eleven** ordinary exact-vtable queries.
Every non-target v14 inference row must remain byte-for-byte JSON-equivalent in
the runner's post-check.

The proof boundary remains `exact_packaged_apk_normal_code_execution`:
`whole_program_alias_writes_excluded=false` is preserved. Injected code,
debugger writes, memory corruption and modified binaries are outside scope.

## Reproduce on Manjaro

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only

bash tools/ml/run_nevergone_controlsbutton_slot80_field_recovery_v15.sh \
  /path/to/com.hippiegame.nevergone.apk \
  ~/nevergone-controls-field-alias-evidence.tar.gz \
  ~/nevergone-controls-alias-operand-roots.tar.gz
```

Output archive:

`~/nevergone-controlsbutton-slot80-field-recovery-v15.tar.gz`

The archive contains v15 inference/query/resolution/failure outputs, the
regenerated 17-call receiver slices, factory-field candidates and the gated
v14 stability artifact. It does not contain original APK/ELF bytes or
decompiler C text.
