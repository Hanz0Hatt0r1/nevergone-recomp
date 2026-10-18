# Frozen router gate availability for v10

Status: **not evaluated**. The stored v10 results do not prove the independent
router gate, so they are not reported as a precision/coverage pass.

The v10 candidate's 0.97301 accepted precision and 0.560759 coverage are from
outer-fold crossfit calibration. The nominal `dataset-4class` test split is not
independent for this router: all 808 test rows occur in the 9,118-row v5.2 OOF
population used in its calibration. The identity-disjoint `unseen500-v10`
corpus has no semantic labels. The v70 manual label refresh adds 183 identities
beyond the OOF population, but only one has a `code_domain` label.

Therefore accepted precision, coverage, critical false-accept rate, and
evidence-conflict acceptance on a frozen labeled holdout remain unavailable.
`availability.json` records only aggregate counts and SHA-256 hashes; it does
not contain function identities, addresses, labels, or local paths.
The [frozen analysis KPI dashboard](../nevergone-kpi-dashboard-v1/README.md)
summarizes the router gate status alongside the receiver coverage target.

## Reproduce the availability audit

With the retained Nevergone ML artifacts present locally:

```bash
python3 tools/ml/audit_nevergone_frozen_router_gate_availability.py \
  ~/.local/share/nevergone-ml-v2 \
  --output docs/evidence/frozen-router-gate-availability-v10/availability.json
```

When a separate labeled gate population exists, route it with the frozen model,
add consumer-reviewed `critical_case` and `evidence_conflict_detected` flags,
then use a thin consumer adapter to normalize Nevergone's legacy router output
and bind its exact calibration population. The legacy v10 format cannot be
passed directly to the generic
[`revenginstr-accept-router-gate`](https://github.com/Hanz0Hatt0r1/revenginstr/tree/0d05223391729fcb2dec2d7187a371d0ba84d690) command. The normalized input must remain identity- and family-disjoint from router calibration.
