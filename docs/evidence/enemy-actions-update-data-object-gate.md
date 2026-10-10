# EnemyActions `updateData()` object-gate evidence

Target: clean-room reconstruction of original Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Native control flow

After the recovered resource-loading slice, native reaches `0x2ac440..0x2ac458`:

- reads system word `+0x250`;
- when `+0x250 == 0`, loads the system vtable and calls virtual slot `+0xcc`, retaining the returned pointer;
- when `+0x250 != 0`, it does not call that virtual slot and instead sets the retained pointer to null.

Native then walks the primary frame array until the current `+0x294` index and performs the recovered `ActionFrameData+0x68 -> spriteFrameByName()` request at `0x2ac476..0x2ac48a`.

Immediately after that request, `0x2ac48c..0x2ac490` tests the pointer retained from the earlier virtual slot. A null pointer branches to `0x2ad2f6`, bypassing the large object-update block. Therefore:

- only `+0x250 == 0` can request the `vtable+0xcc` object;
- a null result bypasses the later object-update block;
- every nonzero `+0x250` mode also bypasses that block because native supplies null without making the virtual call.

The semantic identity of virtual slot `+0xcc` and its pointee is intentionally not inferred here.

## Reconstruction

`apply_update_data_object_gate(document, state, virtual_slot_cc_result_nonnull)`:

- reuses the proven updateData readiness/current-frame entry contract;
- reports whether native would call virtual slot `+0xcc`;
- accepts only the external call's null/non-null observation, not a pointer or object model;
- reports whether control would enter or bypass the later object-update block;
- leaves all offset-named runtime state unchanged.

For malformed project-owned state that fails the recovered updateData entry checks, the helper stops before representing this later gate.

## Outside scope

No semantics are assigned to virtual slot `+0xcc`. The large object-update body after the non-null branch, including cut-object arrays, system `+0x27c`, transform/movement, visibility and render-state writes, remains to be reconstructed in smaller bounded slices.

No proprietary payload or decompiler-derived source is included.
