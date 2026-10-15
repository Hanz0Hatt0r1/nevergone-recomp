# Production routing v8: monotone correctness meta gate

v8 is an evaluation-only continuation of v7 correctness meta routing.

## Why v8 exists

The real v7 run reached accept precision `0.9736`, but accept coverage fell to `0.4441` because v7 rebuilt routing directly from the correctness score. That policy could promote legacy REVIEW/ESCALATE rows into ACCEPT. In the real run this caused GAME accepts to increase from `92` to `248`, while GAME accept precision fell from `0.9348` to `0.8589`.

The nested legacy router itself was already close to the production precision target:

```text
accept precision 0.9696
accept coverage  0.5809
```

Therefore v8 keeps legacy routing as the upper bound on automation and lets meta evidence only make a decision more conservative.

## Monotone policy

```text
legacy ACCEPT + strong correctness score -> ACCEPT
legacy ACCEPT + medium score             -> REVIEW
legacy ACCEPT + weak score               -> ESCALATE

legacy REVIEW + sufficient score          -> REVIEW
legacy REVIEW + weak score                -> ESCALATE

legacy ESCALATE                            -> ESCALATE
```

A meta decision can never promote an action. The evaluator reports `promotions`; it must remain zero.

## Leakage barriers

For every outer family-held-out fold:

1. Build inner family-grouped OOF base predictions with the v5.2 representation.
2. Learn the legacy confidence/similarity routing policy from those inner predictions.
3. Build sanitized structured correctness features.
4. Produce another family-grouped meta-OOF correctness score inside outer-train.
5. Select monotone accept/review thresholds only from those meta-OOF rows and their legacy actions.
6. Fit the final correctness meta-model on outer-train.
7. Apply the frozen legacy policy, correctness model and monotone thresholds once to untouched outer-test families.

Outer-test labels never participate in fitting or threshold selection.

## Threshold objective

Defaults:

```text
target accept precision       >= 0.97
target review precision       >= 0.80
max accept coverage loss      <= 0.03 relative to nested legacy
```

Among feasible policies, v8 maximizes accept coverage first, then accept precision and review precision.

If no feasible monotone policy exists in an outer fold, that fold falls back to the legacy routing policy.

## Run

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only
source ~/.venvs/nevergone-ml/bin/activate

bash tools/ml/run_nevergone_monotone_correctness_routing_v8.sh \
  ~/.local/share/nevergone-ml-v2 \
  | tee ~/.local/share/nevergone-ml-v2/logs/v8-full-run.txt
```

No Ghidra rerun is required.

Outputs:

```text
~/.local/share/nevergone-ml-v2/evidence-v8-monotone-correctness-routing/summary.json
~/.local/share/nevergone-ml-v2/evidence-v8-monotone-correctness-routing/oof_monotone_correctness_routing.jsonl
```

## Promotion gate

The main target is selective production quality rather than class relabeling:

```text
accept precision >= 0.970
accept coverage  >= 0.55 preferred
promotions       = 0
v5.2 reproduction rate = 1.0
```

A result near legacy coverage (`~0.58`) while crossing `0.97` precision is preferable to the v7 result (`0.4441` coverage), even if v7 had slightly higher raw accept precision.
