# Protobuf nested receiver recovery v25

## Scope

v20 exposed four unsafe caller-derived receiver classifications in `PVEDATA::equipToolsListMsg::SharedDtor` at `libcocos2dcpp.so:00299d90`. v22 mapped those four slot-`0x4` receivers exactly to owner fields `+0x8`, `+0xc`, `+0x10`, and `+0x14`. v23/v24 then recovered class-wide write and alias evidence, including the namespace correction required for Ghidra's unqualified `equipToolsListMsg *` display of `this`.

v25 converts only this protobuf cluster from evidence into receiver recovery. It does not relax the unresolved `BeatenEffect` or `CCJoyStick` gates.

## Exact native lifecycle proof

For each of the four fields, the accepted proof requires all of the following native conditions:

- the v20 inference row is still `unresolved_receiver_class` and dispatches through slot `0x4`;
- the v22 structural receiver slice is canonical, complete, and maps the call to exactly one of `+0x8/+0xc/+0x10/+0x14`;
- the direct write inventory for that field is exactly `mutable_titemN`, `SharedCtor`, `Swap`, and `InitAsDefaultInstance`;
- `SharedCtor` supplies the unique null initialization;
- `mutable_titemN` calls `operator.new(0x24)`, immediately constructs `PVEDATA::equipToolsDataMsg` on that exact returned pointer, and stores that pointer into the field;
- `InitAsDefaultInstance` stores the exact `PVEDATA::equipToolsDataMsg::default_instance` result;
- `Swap` performs a reciprocal same-offset transfer between two `equipToolsListMsg` objects, so it cannot introduce a different field class;
- no target field address escapes and no unresolved owner-derived STORE exists in the corrected class-wide owner flow;
- the remaining whole-owner call frontier exactly matches the observed generated/base lifecycle calls, and the remaining owner-derived unknown calls are restricted to generated operations parameterized by `PVEDATA::equipToolsDataMsg`;
- `StaticVtableSlotCatalog/3` contains the unique `PVEDATA::equipToolsDataMsg` vtable `libcocos2dcpp.so:008f32b0` at slot `0x4`, targeting `libcocos2dcpp.so:00299b1c` (`PVEDATA::equipToolsDataMsg::~equipToolsDataMsg`) with no runtime mismatch.

The recovered schema is corroborative only. The promotion is based on native p-code, native write inventory, corrected owner flow, and the static vtable catalog.

## Result

The real v24 evidence satisfies this gate for all four calls in `00299d90`. v25 therefore promotes exactly calls `0..3` to:

- class: `PVEDATA::equipToolsDataMsg`
- vtable: `libcocos2dcpp.so:008f32b0`
- dispatch slot: `0x4`
- target: `libcocos2dcpp.so:00299b1c`

The corrected unresolved receiver frontier decreases from 14 to 10. The remaining rows are seven `BeatenEffect::setOpacity` calls, one `CCJoyStick::timerUpdate` call, and two `CCJoyStick::setDockTexture` calls.

## Next targeted evidence

v22/v24 also revealed previously unhandled non-null writes in two `BeatenEffect` helper functions. The v25 runner therefore exports full high-p-code for:

- `002a3a20` — `BeatenEffect::createBeatenBloodPointEffect02`, which writes the region containing `+0x158`;
- `002a3c74` — `BeatenEffect::createBeatenBloodPointEffect`, which contains computed/looped writes relevant to `+0x14c/+0x150/+0x154`.

These exports are evidence for a later stage. v25 does not use them to promote any `BeatenEffect` receiver.

## Run

```bash
bash tools/ml/run_nevergone_protobuf_nested_receiver_recovery_v25.sh
```

The diagnostic bundle is written to:

```text
~/nevergone-protobuf-nested-recovery-v25.tar.gz
```

The runner preserves a partial archive on failure. All v25 proof metadata remains analysis-only and carries `affects_routing=false`.
