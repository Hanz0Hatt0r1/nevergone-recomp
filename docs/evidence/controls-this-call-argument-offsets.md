# P1A: Controls-relative offsets for all 78 this-derived call arguments

## Why this probe exists

The earlier 38-method Controls inventory recorded **78** calls where one
argument normalized to high-P-code parameter zero, but it intentionally did
not preserve the constant byte offset of that argument. That is too coarse for
alias reasoning: `Controls*`, `Controls+0x104` and an embedded `CCPoint` can
all be described as parameter-zero-derived.

The exact prior distribution is pinned before Ghidra runs:

| Callee category | Calls |
| --- | ---: |
| `cocos2d::CCPoint` subobject candidate | 43 |
| `Controls` method candidate | 21 |
| Cocos base-method candidate | 6 |
| `ControlsButton::create` callback object | 3 |
| unresolved `CALLIND` | 3 |
| `CCCallFuncN::create` callback factory | 1 |
| `operator delete` | 1 |

The selected argument is arg0 for 57 calls, arg1 for 18, and arg2 for 3.

## New evidence

`plan_nevergone_controls_this_call_arguments.py` reads the exact
`NEVERGONE.ControlsFieldAliasTriage/1` JSONL and emits an immutable 78-site
plan with function entry, P-code operation address/time, call target token,
callee classification and selected argument index.

`ExportControlsThisCallArgumentOffsets.java` then revisits those exact P-code
operations read-only. Its address-expression decoder deliberately matches the
one used by the original Controls inventory. For the selected argument it
records:

- normalized root token;
- constant byte offset from that root;
- high-P-code parameter index;
- expression kind.

The strict auditor requires every planned call to keep the same target token,
call kind, callee category, selected argument index, and
`root_parameter_index=0`. It additionally requires the byte offset to resolve.
The result splits the evidence into:

1. exact `Controls* this` (`offset=0`);
2. non-zero addresses relative to `Controls`, which may represent base
   subobjects, embedded `CCPoint` values or fields;
3. unresolved offsets, which fail a strict run.

This split is evidence for the next interprocedural escape audit. It does not
by itself prove that a callee writes through the pointer.

## Run on Manjaro

Use the original 38-method evidence archive and the same original APK:

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only

bash tools/ml/run_nevergone_controls_this_call_argument_probe.sh \
  ~/nevergone-controls-field-alias-evidence.tar.gz \
  /path/to/com.hippiegame.nevergone.apk
```

The runner first revalidates the old archive against the exact original ELF,
then builds the 78-call plan, runs Ghidra headlessly and performs the strict
audit. It creates:

`$HOME/nevergone-controls-this-call-arguments.tar.gz`

The archive contains only derived evidence:

- `controls-field-alias-triage.jsonl`;
- `controls-this-call-argument-plan.tsv`;
- `evidence/controls-this-call-argument-offsets.tsv`;
- `controls-this-call-argument-offset-audit.jsonl`;
- `controls-this-call-argument-offset-audit.txt`.

No APK, ELF, assets or decompiler C text are included. The runner preserves
raw evidence when the strict audit fails so a changed Ghidra representation
can be diagnosed.

## Safety boundary

Even a complete 78-site offset result is not a whole-program write proof.
`whole_program_alias_writes_excluded=false`, `receiver_class_proven=false`,
`affects_routing=false`. No runtime or production dispatch code is modified.
