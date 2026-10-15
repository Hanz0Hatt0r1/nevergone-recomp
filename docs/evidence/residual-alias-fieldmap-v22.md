# Residual receiver alias + field-map v22

## Why this stage exists

The v20 parameter-root safety correction exposed 14 unresolved virtual receivers. Twelve had previously inherited the type of an owner/caller parameter even though the receiver itself was a value loaded from a field. v21 then showed that five `BeatenEffect` fields (`+0x128`, `+0x12c`, `+0x130`, `+0x134`, `+0x138`) have observed non-null writes only from `cocos2d::CCSprite::create*`, while `BeatenEffect +0x14c/+0x150/+0x154/+0x158` and `CCJoyStick +0x14c` have only observed null initialization.

Direct parameter-relative STOREs are not enough to prove field stability: an alias of `this`, or an address of a field passed to another function, could still replace the value. v22 therefore remains evidence-only and closes that gap before any new receiver class is promoted.

## Evidence

`ExportClassOwnerAliasInventory.java` scans every exact Ghidra method owned by `BeatenEffect` and `CCJoyStick`. It exports every high-P-code `STORE`, `CALL`, and `CALLIND`, exact parameter-zero affine offsets, unresolved expressions that still contain parameter zero, and direct call targets when available.

`audit_nevergone_residual_owner_alias_frontier_v22.py` joins that inventory with the v21 direct field writes and the structural slice for `BeatenEffect::setOpacity`. The exact call-to-field mapping is:

- call 0 -> `BeatenEffect+0x130`
- call 1 -> `+0x134`
- call 2 -> `+0x138`
- call 3 -> `+0x12c`
- call 4 -> `+0x128`
- call 5 -> `+0x158`
- call 6 -> loop over `+0x14c/+0x150/+0x154`

A `CCSprite` candidate is only marked proven if every observed non-null direct write is a known `CCSprite::create*` return **and** the class-wide alias inventory has no unresolved parameter-zero STORE, whole-owner external call escape, target-field-address call escape, or unknown parameter-zero-derived call argument. Otherwise the row remains unresolved.

The same runner also reslices the six newly exposed calls at `00299d90` and `002a651c` and records exact Ghidra function identities. `audit_nevergone_newly_unresolved_field_shapes_v22.py` summarizes root opcodes and plausible field offsets without assigning a class.

## Run

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only
bash tools/ml/run_nevergone_residual_alias_fieldmap_v22.sh
```

Output archive:

```text
~/nevergone-residual-alias-fieldmap-v22.tar.gz
```

This stage does not change production routing and does not infer a receiver class unless the bounded alias proof is complete. `affects_routing=false` remains explicit in generated audit records.
