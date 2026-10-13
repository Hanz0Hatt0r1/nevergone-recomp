# Dispatch runtime target evidence

This lane converts structural CALLIND equivalence into runtime target observations without changing routing.

## 1. Build a bounded trace plan

Generate the seed template first with `propagate_nevergone_dispatch_target_evidence.py`, then select only repeated exact ordered groups:

```sh
python3 tools/ml/build_nevergone_dispatch_runtime_probe_plan.py \
  /tmp/nevergone-dispatch-target-seed-template.jsonl \
  --output /tmp/nevergone-dispatch-runtime-plan.json
```

The plan contains exact `libcocos2dcpp.so` Thumb call-site offsets. It does not contain target predictions.

## 2. Observe targets with unidbg

`NevergoneHarness --trace-dispatch-targets PLAN` installs observation-only breakpoints. The tracer never calls any selected function itself. Pair the trace option with an existing bounded workload, for example:

```sh
cd tools/unidbg
./run.sh --trace-dispatch-targets /tmp/nevergone-dispatch-runtime-plan.json \
  --probe-enemy-actions-ctor > target/dispatch-runtime.log 2>&1
```

Other existing bounded workloads can be substituted. If a workload does not naturally reach a selected call site, no runtime-target row is emitted for that site.

Each successful hit emits an `EVIDENCE` JSON object with:

- source function/call index/call-site offset;
- ordered equivalence group and slot group;
- actual Thumb `BLX <reg>` instruction;
- runtime target register value;
- normalized target module/offset when the target belongs to a loaded module;
- r0-r3 values at the call site.

## 3. Convert observations into verified seeds

```sh
python3 tools/ml/convert_nevergone_dispatch_runtime_trace_to_seeds.py \
  tools/unidbg/target/dispatch-runtime.log \
  --output /tmp/nevergone-dispatch-target-seeds.jsonl
```

If the same call site is observed with more than one runtime target, all targets are preserved as separate verified seeds.

## 4. Propagate candidates conservatively

```sh
python3 tools/ml/propagate_nevergone_dispatch_target_evidence.py \
  /tmp/nevergone-internal-unseen-500.analysis-equivalence.jsonl \
  /tmp/nevergone-dispatch-target-seeds.jsonl \
  --output /tmp/nevergone-internal-unseen-500.analysis-targets.jsonl \
  --candidates-output /tmp/nevergone-dispatch-target-candidates.jsonl
```

A runtime observation is verified only for the original call site. Peers receive candidate evidence only. Exact ordered-position propagation is stronger than same-slot-group propagation, but neither automatically resolves a peer target. Conflicting seeds remain explicit conflicts.
