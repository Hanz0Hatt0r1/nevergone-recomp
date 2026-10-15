# Production routing v9: accept-only error detector

## Motivation

The real v8 run kept class predictions unchanged and enforced monotone routing, but
its all-row correctness score barely improved ACCEPT precision:

- legacy ACCEPT: 5388 examples, precision 0.9599, coverage 0.5909;
- v8 ACCEPT: 5369 examples, precision 0.9603, coverage 0.5888;
- only 3 of 19 removed ACCEPT rows were errors.

At the same time v8 successfully improved REVIEW precision from 0.7911 to 0.8584.
This means the generic correctness score contains useful routing signal, but it is
not specialized enough for errors inside the already selective ACCEPT population.

The error inventory also shows that most ACCEPT errors are concentrated in two
predicted classes: engine (99) and media_library (94), versus runtime_library (15)
and game (8).

## v9 experiment

v9 trains a second-stage correctness model **only on leakage-safe legacy ACCEPT
rows**. It does not retrain the P-code classifier and does not rerun Ghidra.

Inputs:

- v5.2 OOF predictions: held-out class probabilities, family similarity and
  contrast margin;
- v8 OOF routing rows: outer-fold identity and leakage-safe legacy action;
- v5.2 evidence: sanitized numeric structural/receiver/callgraph evidence.

For each target outer fold:

1. select legacy ACCEPT rows from all other outer folds;
2. generate accept-meta OOF scores by leaving each remaining outer fold out in
   turn;
3. choose a keep threshold on those accept-meta OOF scores only;
4. fit the final accept correctness meta-model on all non-target ACCEPT rows;
5. score target-fold ACCEPT rows once;
6. keep rows above threshold as ACCEPT and downgrade the rest directly to
   ESCALATE.

Legacy REVIEW and ESCALATE actions never change. Class predictions never change.
The router therefore cannot promote any row.

## Default gate

Threshold selection requires:

- ACCEPT precision >= 0.97;
- absolute ACCEPT coverage loss <= 0.04;
- at least 100 retained ACCEPT examples in the calibration population.

If no threshold satisfies the constraints, the target fold falls back to the
legacy ACCEPT set unchanged.

## Promotion criterion

The experiment is useful only if held-out aggregate routing reaches approximately:

- ACCEPT precision >= 0.97;
- ACCEPT coverage >= 0.55;
- promotions = 0;
- removed wrong ACCEPT rows materially exceed removed correct ACCEPT rows.

If this also fails, the remaining ACCEPT errors need stronger evidence/features,
not further threshold tuning.
