# P1A: Controls field alias triage from the returned Ghidra archive (2026-10-14)

## Evidence and results

Input: `nevergone-controls-field-alias-evidence.tar.gz`, the exact original
Nevergone ARMv7 ELF (SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`)
and the read-only, 38-method Ghidra `Controls` field-write inventory.

The bounded audit completed with no structural errors: **38/38** methods,
**167** high-P-code `STORE` operations and **293** `CALL`/`CALLIND`
operations. It confirmed only the three direct
`Controls+0x12c/0x130/0x134` `ControlsButton::create` stores at Ghidra
addresses `002a8120`, `002a8172`, `002a81c2`.

The 46 stores with unknown base-parameter identity divide into:

| Unresolved base provenance | Count |
| --- | ---: |
| Register base (high-P-code parameter identity not established) | 37 |
| Base defined by `LOAD` | 9 |
| Unknown stores with an immediate offset equal to `0x12c`, `0x130` or `0x134` | **0** |

All 46 have a recovered constant immediate offset *different* from
those three fields. This does **not** exclude an alias: a loaded pointer
or an adjusted base could still address the same memory cell.

Among the **78** calls previously marked as receiving an argument
derived from the decompiler's parameter zero, exact ELF-symbol target
matching gives:

| Called function category | Count |
| --- | ---: |
| `cocos2d::CCPoint` constructor/copy/assignment | 43 |
| `Controls` methods | 21 |
| `cocos2d::CCNode` / `CCLayer` methods | 6 |
| `ControlsButton::create` callback factory | 3 |
| `CALLIND` unresolved | 3 |
| `CCCallFuncN::create` | 1 |
| `operator delete` | 1 |

All 75 direct targets matched an **exact original ELF function symbol**.
The 43 `CCPoint` calls are likely associated with point subobjects,
but the earlier exporter did not record the offset of the argument
from parameter zero. **Do not treat 78 as 78 proven escapes of the
unadjusted `Controls*` pointer.** The classification is by callee
symbol only.

## Reproducible triage

The new `tools/ml/triage_nevergone_controls_field_alias_evidence.py`
takes the original archive and APK or extracted ELF, verifies the
pinned SHA, method manifest, native direct field witnesses, and the
Ghidra audit's bounded operation counts. It resolves direct call
targets using `.dynsym` without a network lookup and emits a
conservative classification with all safety flags false.

```bash
cd `/nevergone-recomp
git switch main
git pull --ff-only

python3 tools/ml/triage_nevergone_controls_field_alias_evidence.py \
  `/nevergone-controls-field-alias-evidence.tar.gz \
  /path/to/com.hippiegame.nevergone.apk \
  --details-output /tmp/nevergone-controls-field-alias-triage.jsonl \
  --plan-output /tmp/nevergone-controls-field-next-probe-plan.tsv
```

The 15 next-probe sites comprise **9** `LOAD`-based unknown STORE
bases, **3** indirect calls, and **3** callback-factory calls.
They are candidates for bounded Ghidra pointer-base/argument-offset
slices; the plan is **not** a dispatch recovery or a field-write
exclusivity proof.

`all_alias_writers_excluded=false`, `receiver_class_proven=false`,
`affects_routing=false`. No production routing or runtime code changed.
