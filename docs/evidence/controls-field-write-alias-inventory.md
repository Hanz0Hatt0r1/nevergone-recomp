# P1A: Controls field writer and alias inventory (2026-10-14)

## Why the additional probe is needed

The earlier exact native witness links eight virtual calls in
`Controls::showControlsAttackKey(int)` to fields `+0x12c`, `+0x130` and
`+0x134`, initialized in `Controls::init()` by
`ControlsButton::create()`. It does not exclude later replacement of those
field values. The old GameSceneUI/Controls caller-argument convergence is
still quarantined.

A halfword-aligned scan of the **entire original ARMv7 ELF** (SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`)
finds **575 encoding candidates** for Thumb-2 `STR.W Rt,[Rn,#imm12]`
with immediate offsets `0x12c`, `0x130`, or `0x134`. Of these, 64
have no containing exported `STT_FUNC` owner. These are a **coarse screen**,
not 575 writes to a Controls object: offsets are shared across unrelated
classes, and halfword scanning may include non-instruction bytes.

The original ELF has 40 `Controls` function symbol records at 38
unique method entrypoints (constructor and destructor aliases share
addresses). Among the encoding candidates within those 38 methods,
**exactly three** are direct field stores:

| ELF STR.W | Ghidra address | Field | Registers | Method |
| --- | --- | --- | --- | --- |
| `0x298120` | `002a8120` | `0x12c` | `r0 -> [r4]` | `Controls::init` |
| `0x298172` | `002a8172` | `0x130` | `r0 -> [r4]` | `Controls::init` |
| `0x2981c2` | `002a81c2` | `0x134` | `r0 -> [r4]` | `Controls::init` |

These three were also verified against exact Thumb factory-to-store
instruction spans by the preceding witness checker. For comparison,
`CCJoyStick::setHitAreaWithRadius` writes `CCJoyStick+0x12c` at
ELF `0x296568`; its matching offset is **not** a Controls write.

## New two-stage evidence

1. `audit_nevergone_controls_field_native_store_inventory.py` takes the
   original APK or extracted ELF, verifies its exact SHA and existing
   constructor/vtable witnesses, inventories all Thumb encoding candidates,
   and emits the exact 38-entry `Controls` method manifest. No APK or
   proprietary instruction bytes are committed.
2. `ExportControlsFieldWriteAliasInventory.java` decompiles **every one**
   of those 38 method addresses read-only, emits one summary per method
   (including decompile failures), plus **every high-P-code STORE and
   CALL/CALLIND** in each method. It conservatively normalizes
   `base+constant` addresses, identifies the decompiler's parameter-zero
   base where possible, and records calls receiving a pointer derived from
   parameter zero.
3. `audit_nevergone_controls_field_write_aliases.py` checks exact
   method-set identity, per-method STORE/CALL coverage, unknown address
   stores, potential `this` escapes and whether the three known
   `Controls::init` stores are the only proven direct `this+field` writers.
   Unexpected writers and incomplete decompilation fail a strict audit.

A complete bounded scan does **not** establish whole-program alias
completeness. Unknown stores, register-indexed writes, pointer escapes
and functions outside the 38 methods may still mutate the object.
The result is diagnostic: `all_alias_writers_excluded=false`,
`receiver_class_proven=false`, `affects_routing=false`. It cannot
promote the three quarantined v11 calls or any other dispatch target.

## Run on Manjaro

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only

bash tools/ml/run_nevergone_controls_field_alias_probe.sh \
  /path/to/com.hippiegame.nevergone.apk
```

The runner uses the existing Ghidra headless project
(`GHIDRA_HOME`, `GHIDRA_PROJECT_DIR`, `GHIDRA_PROJECT_NAME`,
`GHIDRA_PROGRAM` are configurable). It exports:

- `controls-field-native-store-inventory.jsonl`
- `controls-field-native-methods.tsv`
- `evidence/controls-field-write-aliases.tsv`
- `controls-field-write-alias-audit.jsonl`
- `controls-field-write-alias-audit.txt`

They are packaged in
`$HOME/nevergone-controls-field-alias-evidence.tar.gz`.
The archive is created even if the strict audit finds a missing method
or an unexpected writer; in that case the runner exits nonzero.
Upload that archive for the next receiver-provenance step.

No game runtime code or production routing is modified.
