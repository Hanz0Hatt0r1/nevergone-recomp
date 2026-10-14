# P1A: Controls full-this external callee frontier

## Purpose

The exact Ghidra and native argument-offset probes agree that the 78 broad
`Controls*`-derived call arguments split into 35 unadjusted `Controls*` calls
and 43 non-zero embedded `CCPoint` addresses. This probe advances only the 35
unadjusted calls.

Twenty-one of those 35 are calls to other `Controls::*` methods. They are
already inside the existing complete 38-method local STORE inventory, so this
stage does not re-export them. The remaining external surface is exactly 14
sites:

- 6 direct Cocos base-method calls;
- 3 direct `ControlsButton::create` callback-object calls;
- 1 direct `CCCallFuncN::create` callback-factory call;
- 1 direct `operator delete` call;
- 3 `CALLIND` sites on `Controls* this`.

## Evidence exported

`plan_nevergone_controls_full_this_callee_frontier.py` is pinned to the exact
original ELF SHA from `ControlsFieldAliasTriage/1`, the complete
`ControlsThisCallArgumentOffsetAudit/1`, and the immutable 78-site source plan.
It emits only those 14 external full-this sites.

For each of the 11 direct sites,
`ExportControlsFullThisCalleeParameterUses.java` decompiles the exact callee and
starts from the corresponding high-P-code parameter. A bounded use-def walk
tracks transparent copies and constant pointer arithmetic and records:

- direct STORE destinations relative to the incoming pointer;
- STOREs that retain the pointer as a value;
- direct/indirect calls that forward the pointer;
- pointer returns;
- dereference reads;
- nontransparent/opaque uses.

The three source `CALLIND` sites stay explicit frontier rows; this stage does
not invent direct targets for them.

`audit_nevergone_controls_full_this_callee_frontier.py` verifies every direct
site was completely exported and reports whether any direct callee writes
exactly `Controls+0x12c`, `+0x130`, or `+0x134`. Retention, forwarding, return,
opaque uses and source `CALLIND`s remain in the next frontier.

This is deliberately not a whole-program alias-exclusion proof. Even a clean
first hop only narrows the next recursion step.

## Reproduce

After generating `~/nevergone-controls-this-call-arguments.tar.gz`:

```bash
cd ~/nevergone-recomp
git switch probe-controls-full-this-callee-frontier
git pull --ff-only

bash tools/ml/run_nevergone_controls_full_this_callee_frontier_probe.sh \
  ~/nevergone-controls-this-call-arguments.tar.gz
```

Output archive:

`~/nevergone-controls-full-this-callee-frontier.tar.gz`

No APK or ELF bytes are included. `affects_routing=false`.
