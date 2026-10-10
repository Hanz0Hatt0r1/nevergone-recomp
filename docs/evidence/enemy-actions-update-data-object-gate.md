# EnemyActions `updateData()` retained-object gate evidence

Target: clean-room reconstruction of original Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Native control flow

After the recovered resource-loading slice, native reaches `0x2ac440..0x2ac458`:

- reads system word `+0x250`;
- when `+0x250 == 0`, loads the system vtable and calls virtual slot `+0xcc`, retaining the returned pointer in a callee-saved register;
- when `+0x250 != 0`, it does not call that virtual slot and instead retains null.

Native then walks the primary frame array until the current `+0x294` index and performs the recovered `ActionFrameData+0x68 -> spriteFrameByName()` request at `0x2ac476..0x2ac48a`.

Immediately after that request, `0x2ac48c..0x2ac490` tests the pointer retained from the earlier virtual slot. A null pointer branches to `0x2ad2f6`, which rejoins the common downstream frame/transform path at `0x2ac600`. Therefore the earlier wording that null bypassed the entire later object-update body was too broad.

The exact bounded conclusion is:

- only `+0x250 == 0` can request the `vtable+0xcc` object;
- a null result bypasses the **cut-processing slice** beginning at `0x2ac494`;
- every nonzero `+0x250` mode likewise bypasses cut processing because native supplies null without making the virtual call;
- those paths still continue into the common downstream update path rather than returning from the function.

The semantic identity of virtual slot `+0xcc` and its pointee is intentionally not inferred here.

## Reconstruction

`apply_update_data_object_gate(document, state, virtual_slot_cc_result_nonnull)`:

- reuses the proven updateData readiness/current-frame entry contract;
- reports whether native would call virtual slot `+0xcc`;
- accepts only the external call's null/non-null observation, not a pointer or object model;
- reports `cut_processing_gate_open` only for the mode-0/non-null case;
- reports `cut_processing_bypassed` for valid updateData frames that skip the cut slice;
- reports `continues_to_common_downstream` for every valid frame, including null/nonzero-mode paths;
- leaves all offset-named runtime state unchanged.

For malformed project-owned state that fails the recovered updateData entry checks, the helper stops before representing this later gate.

## Relationship to `+0x278`

Fresh disassembly of `0x2ac494+` shows that `system+0x278` is read only after this retained-object gate succeeds. The `+0x278` comparison chooses between the EnemyObject-owned cut array and the system default `+0x27c` array; that source-selection contract is documented separately.

No proprietary payload or decompiler-derived source is included.
