# P1A: exact native offsets for all 78 Controls-derived call arguments

## Result

The original ARMv7 `libcocos2dcpp.so` SHA-256 is pinned to
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.
Starting from the 78 calls in `NEVERGONE.ControlsFieldAliasTriage/1`, the
native cross-check recovers the selected argument offset relative to the
`Controls*` object for every call.

The split is exact:

- **35** calls receive the unadjusted `Controls*` (`+0x0`);
- **43** calls receive non-zero addresses inside `Controls`;
- **0** offsets remain unresolved.

The important separation is categorical:

| Callee category | Offset result |
| --- | --- |
| 43 `cocos2d::CCPoint` calls | **all non-zero** |
| 21 `Controls` method calls | all `+0x0` |
| 6 Cocos base-method calls | all `+0x0` |
| 3 `ControlsButton::create` callback-object calls | all `+0x0` |
| 3 unresolved `CALLIND` sites | all `+0x0` |
| 1 `CCCallFuncN::create` callback factory | `+0x0` |
| 1 `operator delete` | `+0x0` |

Therefore all 43 previously broad `CCPoint` candidates are **embedded point
subobjects**, not escapes of the complete `Controls*` object. Their offsets
are limited to:

`+0x168`, `+0x190`, `+0x198`, `+0x1a0`, `+0x1a8`, `+0x1b0`, `+0x1b8`,
`+0x1c0`, `+0x1c8`, `+0x1d0`, `+0x1d8`, `+0x1e0`.

The non-zero arguments are verified against exact Thumb-2
`ADD.W r0/r1,r4,#offset` instruction encodings. One constructor preparation
at `0x29697e` is separated from its call by vtable initialization; the whole
native interval through the first `CCPoint` constructor call is SHA-pinned.
Every one of the 78 call sites is also checked to remain a Thumb BL/B.W or
BLX-register instruction of the expected direct/indirect kind.

## Consequence for alias analysis

The apparent interprocedural surface contracts from 78 broad
"parameter-zero-derived" arguments to **35 actual unadjusted Controls
pointers**. Of those 35:

- 21 are calls back into `Controls` methods already covered by the complete
  38-method local STORE inventory;
- the 3 `ControlsButton::create` retained callback paths are closed by the
  exact callback analysis in `controls-retained-alias-closure.md`;
- the 3 `CALLIND` sites were identified as Controls-vtable calls;
- the remaining external surfaces are the Cocos base methods,
  `CCCallFuncN::create`, and `operator delete`.

This substantially narrows the next whole-program alias audit. It still does
**not** prove what external callees do with the pointer.

## Reproduce

After generating the existing Controls alias triage JSONL:

```bash
python3 tools/ml/audit_nevergone_controls_this_call_native_offsets.py \
  $HOME/.cache/nevergone-recomp/receiver-provenance/controls-field-alias-triage.jsonl \
  /path/to/com.hippiegame.nevergone.apk \
  --details-output /tmp/nevergone-controls-this-call-native-offsets.jsonl
```

The companion 78-site Ghidra probe remains useful as an independent
cross-check of the same offsets, but this native result does not require a new
Ghidra run.

Safety remains conservative:
`whole_program_alias_writes_excluded=false`, `receiver_class_proven=false`,
`affects_routing=false`.
