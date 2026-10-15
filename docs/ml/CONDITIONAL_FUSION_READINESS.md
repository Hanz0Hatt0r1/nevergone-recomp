# Conditional fusion readiness gate

The production code-domain classifier v4 is intentionally conservative. A nested
conditional-fusion experiment already tests whether nearest-family transfer can
promote selected classifier `review`/`escalate` rows, but its stdout is an
experiment report rather than a production gate.

This stage converts that untouched outer-family CV report into a fail-closed,
machine-readable readiness artifact. It still does **not** modify a production
classifier or the hierarchical router.

## Why this is separate from the CV evaluator

The nested evaluator learns classifier routing, transfer thresholds and the
conditional promotion thresholds only from inner family-grouped folds. Outer
families are touched once for evaluation. Keeping the readiness audit separate
preserves that evidence boundary and prevents production code from silently
learning from outer validation results.

The audit uses one predeclared policy scope. The default is
`per_label_action`; it does not compare both scopes and select whichever looks
better on the same outer results.

## Default candidate surface

The one-command runner evaluates transfer promotion for:

- `engine`;
- `media_library`;
- `runtime_library`.

`game` is deliberately excluded from the trusted-transfer set in this stage.
The current hierarchy already requires an accepted `game` code-domain result
before invoking the subsystem head. Expanding that boundary should use a
separate cross-head validation experiment rather than treating nearest-family
similarity as semantic proof of `game`.

## Fail-closed gates

`audit_nevergone_conditional_fusion_readiness.py` parses only
`NevergoneNestedConditionalFusionCV/1` output and requires the expected outer
fold set. Candidate labels must have been predeclared as trusted in the CV run.

For each `label/action` policy key the default gate requires:

- at least 10 outer-family promotions;
- outer promotion precision >= 0.95;
- the policy enabled in at least 3 of 5 outer folds;
- minimum inner-policy precision >= 0.95.

Passing policy keys are combined with the existing base classifier accepts.
The default global gate additionally requires:

- at least 10 total promoted functions;
- at least one passing policy key;
- candidate fused precision not below the base accepted precision.

The resulting artifact format is:

`NEVERGONE.ConditionalFusionReadiness/1`

A positive artifact means only:

`ready_for_independent_validation=true`

It explicitly records:

`production_routing_changed=false`

The outer-held-out corpus is still part of model development. Before a policy
key is wired into production routing, validate it on an independent labelled
cross-binary/export set or another genuinely untouched evidence source.

## Run

Use the same labelled dataset and family map used for nested CV:

```bash
cd ~/nevergone-recomp
source /tmp/nevergone-ml-venv/bin/activate

bash tools/ml/run_nevergone_conditional_fusion_readiness.sh \
  /tmp/nevergone-ml-v2/dataset-4class \
  /tmp/nevergone-ml-v2/families.jsonl \
  /tmp/nevergone-ml-v2/conditional-fusion-readiness
```

Outputs:

```text
conditional-fusion-code-domain.log
conditional-fusion-readiness.json
```

A negative readiness result is a valid experiment result and therefore does not
make the runner fail by default. To use the gate in a later validation job that
must fail unless it passes:

```bash
REQUIRE_READY=1 bash tools/ml/run_nevergone_conditional_fusion_readiness.sh \
  /tmp/nevergone-ml-v2/dataset-4class \
  /tmp/nevergone-ml-v2/families.jsonl
```

Thresholds can be overridden through the documented environment variables in
the runner, but changing them after inspecting the outer results invalidates the
claim that the gate was predeclared. For a serious comparison, record the
command and thresholds before running the CV.

## Next decision

If no policy key passes, keep v4 routing unchanged and use new GOLD/cross-binary
labels to improve evidence coverage.

If one or more policy keys pass, do **not** enable them directly. Run those exact
keys unchanged on an independent labelled export. Only an independent pass can
justify a production fusion artifact and an update to
`classify_nevergone_hierarchy.py`.
