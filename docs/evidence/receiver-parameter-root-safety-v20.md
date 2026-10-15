# Receiver parameter-root safety v20

The v20 evidence pass corrects an unsafe assumption discovered while inspecting
the real post-v19 residual receiver data.

## Why v20 exists

The older `parameter_caller_type` recovery used direct-caller argument types as
receiver-class evidence. That is valid only when the virtual receiver itself is
the function parameter. It is not valid when the receiver is a value loaded from
a field owned by that parameter.

Real evidence from `BeatenEffect::setOpacity` at `0x002a47c6` exposed this gap.
Calls 0 through 5 had been classified as `BeatenEffect` from caller evidence,
although structural data flow shows field-loaded receivers. In particular slot
`0x180` has class-dependent meanings, so owner-class transfer through a field is
not safe.

The hardened caller safety audit now requires independent
`NEVERGONE.GhidraTargetReceiverProvenance/1` evidence for every caller-derived
resolution. A row is safe only if the receiver is canonical, call identity and
slot agree, the receiver roots at a function parameter (`parameter_index >= 0`),
and an explicit receiver, when present, matches the structural receiver.

## Forward correction for existing v19 artifacts

`tools/ml/run_nevergone_receiver_parameter_root_safety_v20.sh` regenerates target
receiver provenance and applies the same rule to the existing v19 artifacts. It
must demote all six known `BeatenEffect::setOpacity` caller-derived rows rather
than guessing replacement classes. The two receiver rows already unresolved in
v19 remain unresolved as well.

Run:

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only
bash tools/ml/run_nevergone_receiver_parameter_root_safety_v20.sh
```

The archive is:

```text
~/nevergone-receiver-parameter-root-safety-v20.tar.gz
```

No new class or vtable is inferred by v20 and all new evidence retains
`affects_routing=false`.

## v21 field-write evidence

The next stage does not infer classes either. `ExportClassParameterFieldWrites`
scans all Ghidra functions owned by the exact `BeatenEffect` and `CCJoyStick`
class names and exports exact high-P-code STOREs whose address roots at parameter
0. The stored value is recorded as an exact direct CALL return, function
parameter, constant, loaded value, or other structural root.

The targeted fields are:

- `BeatenEffect +0x128/+0x12c/+0x130/+0x134/+0x138/+0x14c/+0x150/+0x154/+0x158`;
- `CCJoyStick +0x14c`.

Run after v20:

```bash
bash tools/ml/run_nevergone_residual_owner_field_writes_v21.sh
```

The archive is:

```text
~/nevergone-residual-owner-field-writes-v21.tar.gz
```

The v21 runner deliberately preserves the diagnostic archive when one or more
target fields have no exact parameter-0 STORE in the decompiled class methods.
A missing STORE is a negative scientific result, not permission to infer a
receiver class from the method name or slot semantics.
