# P1A: ControlsButton owner-field probe for the final three slot-0x80 sprites

After v15/v16, the old 17-call slot-`0x80` candidate set has only three
unpromoted rows, all owned by `ControlsButton` rather than `Controls`:

- `ControlsButton+0x104` -> `cocos2d::CCSprite`;
- `ControlsButton+0x108` -> `cocos2d::CCSprite`;
- `ControlsButton+0x10c` -> `cocos2d::CCSprite`.

The previous Controls packaged closure cannot be reused for another owner
class. This probe therefore starts a separate, fail-closed `ControlsButton`
field-stability audit.

## Native manifest

`plan_nevergone_controlsbutton_owner_field_probe.py` accepts only the exact
original ARMv7 ELF SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.
It requires 21 `ControlsButton` function symbol records at 19 unique method
entrypoints and emits the exact method manifest.

The same native gate pins all 11 direct Thumb `STR.W` instructions to the
three target offsets:

- zero-initialization stores in both init overloads;
- `CCSprite::create` result stores for `+0x104/+0x108/+0x10c`;
- the two-image overload's second sprite stores.

Any new direct target-offset STORE changes the plan and fails before Ghidra.

## Ghidra inventory

`ExportControlsButtonOwnerFieldAliasInventory.java` decompiles all 19 exact
method entries read-only and emits every high-P-code `STORE`, `CALL` and
`CALLIND`. For stores it records normalized base+constant provenance and the
high-P-code parameter index. For calls it traces every argument and records
arguments derived from parameter zero together with their constant offset.
Unknown roots and unknown argument expressions remain explicit.

`audit_nevergone_controlsbutton_owner_field_probe.py` requires 19/19 method
summaries, exact STORE/CALL counts per method and the complete known set of 11
direct target-field writes. It reports unknown STORE bases and every
parameter-zero-derived call for the next interprocedural closure stage.

This probe is **evidence-only**. Even a structurally complete result emits
`owner_fields_stable=false`, `receiver_class_proven=false`,
`affects_routing=false` until all local unknown stores and retained/external
alias paths have been classified.

## Run on Manjaro

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only

bash tools/ml/run_nevergone_controlsbutton_owner_field_probe.sh \
  /path/to/com.hippiegame.nevergone.apk
```

Output:

`~/nevergone-controlsbutton-owner-field-evidence.tar.gz`

The archive contains the exact native plan, 19-method manifest, raw Ghidra
STORE/CALL inventory and the audit JSONL/text. The APK, ELF bytes and
decompiler C output are not included.
