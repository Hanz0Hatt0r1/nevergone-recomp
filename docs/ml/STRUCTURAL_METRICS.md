# Structural metrics evidence

`tools/ml/extract_nevergone_structural_metrics.py` derives deterministic metrics from the portable P-code view used by the ML pipeline. The metrics are descriptive evidence only and do not change routing actions.

## Included metrics

The extractor reports operation count and opcode diversity, load/store density, direct and indirect call counts, indirect-call ratio, branch/control-flow density, a conservative decision-point count and `cyclomatic_proxy`, compare/arithmetic/bitwise/conversion/pointer counts, SSA merge/copy/slice counts, and omitted-definition coverage proxies.

`cyclomatic_proxy` is intentionally not called exact cyclomatic complexity because branch targets are removed from the portable representation. It is defined as `1 + conditional branches + indirect branches` for non-empty functions.

`definition_coverage_proxy` is `1 - omitted_def_line_ratio`; it describes how often the normalized P-code contains `def:omitted`, not overall decompiler correctness.

## Not included

`fan_in` and distinct `fan_out` require call-graph evidence with stable call targets. Portable P-code intentionally removes call-target identity, so those metrics must be enriched later from the Ghidra call index rather than guessed from the number of call instructions.

## Fresh corpus

```bash
python3 tools/ml/extract_nevergone_structural_metrics.py \
  /tmp/nevergone-internal-unseen-500.jsonl \
  --output /tmp/nevergone-internal-unseen-500.structural.jsonl
```

Attach the evidence to an existing analysis stream without changing `final_action`:

```bash
python3 tools/ml/attach_nevergone_structural_metrics.py \
  /tmp/nevergone-internal-unseen-500.analysis.jsonl \
  /tmp/nevergone-internal-unseen-500.structural.jsonl \
  --output /tmp/nevergone-internal-unseen-500.analysis-structural.jsonl
```
