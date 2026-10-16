# Analysis System Development Roadmap

**Status:** active engineering plan  
**Baseline date:** 2026-10-17  
**Repository baseline:** `main` through `4dd714f55c0853688fab687be8fe83676670781e`  
**Scope:** reusable reverse-engineering analysis stack: evidence extraction, indirect-dispatch recovery, receiver/type/vtable inference, ML classification, selective routing, semantic inference, autonomous analysis orchestration, and cross-project validation.

> This document is an engineering roadmap, not a claim of measured code coverage. Readiness percentages are planning estimates. Every milestone below is gated by reproducible metrics and retained evidence.

## 1. North-star goal

Build an evidence-first analysis system that can take a previously unseen native game binary and autonomously produce a high-confidence, auditable analysis package containing:

1. canonical structural evidence;
2. callgraph and indirect-call evidence;
3. receiver/class/vtable hypotheses with provenance;
4. code-domain and subsystem classification;
5. function-level behavior/effect summaries;
6. semantic role hypotheses and dependency graphs;
7. `accept` / `review` / `escalate` decisions calibrated to measured risk;
8. retained artifacts sufficient for a human or downstream recompilation agent to verify every important conclusion.

The system must prefer **abstention over fabricated certainty**. Increasing coverage is valuable only while high-confidence precision and evidence provenance remain above their safety gates.

The long-term target is not a Nevergone-specific classifier. Nevergone is the current testbed for a generic stack that should transfer to other stripped or partially symbolized native game binaries with minimal project-specific adaptation.

## 2. Current baseline

Current planning snapshot:

| Area | Estimated maturity | Current state |
| --- | ---: | --- |
| Ghidra / P-code / structural evidence extraction | 92-95% | Mature and increasingly routed through shared `revenginstr` exporters |
| Callgraph and indirect-call evidence | 90-95% | Broad static evidence; frozen unseen callgraph coverage has reached 100% in the current testbed |
| Receiver / class / vtable inference | 90-93% | Strong exact and provenance-backed recovery; remaining problem is coverage of hard unseen cases |
| Production code-domain classifier | 83-86% | Useful, but `game` vs `engine` remains the difficult boundary |
| Selective `accept/review/escalate` router | 78-82% | High precision; insufficient automatic coverage |
| Semantic behavior understanding | 60-65% | Main analytical bottleneck |
| End-to-end autonomous analysis | 55-60% | Requires human review for too many high-value semantic decisions |

Important measured baselines retained from the current classifier/router work:

- selective classifier v5.4: balanced accuracy about `0.8336`, macro-F1 about `0.7650`;
- v5.4 `game`: precision about `0.4244`, recall about `0.7058`;
- a more conservative v5.6 operating point raised `game` precision to about `0.7003` and reduced `engine -> game` errors to about `109`, but reduced `game` recall to about `0.5177`;
- selective router v7: precision about `0.9736`, coverage about `0.4441`;
- an earlier nested router operating point reached coverage about `0.5809` at precision about `0.9696`;
- exact retained vtable-slot checks have produced zero mismatches on the current checked cases;
- receiver evidence coverage on a totally unseen slice is still much lower than callgraph coverage and is the main remaining structural-coverage problem.

These values are baselines, not permanent thresholds. Each experiment must emit the exact dataset identity, split identity, model/config identity and retained prediction rows used to calculate its reported numbers.

## 3. Engineering rules

### 3.1 Evidence first

A model prediction is never semantic proof by itself. The system must preserve the distinction between:

- exact static evidence;
- bounded native/runtime evidence;
- inferred structural evidence;
- learned/model predictions;
- human-provided labels;
- interventional probes.

No stage may silently promote a weaker evidence class into a stronger one.

### 3.2 Generic-first implementation

New reusable extraction and analysis primitives should live in or migrate toward `revenginstr` when practical. Nevergone-specific wrappers may remain for compatibility, retained experiments and project-specific joins, but duplicate generic implementations should be removed or explicitly documented.

### 3.3 Leakage-resistant evaluation

All ML and heuristic tuning must preserve:

- normalized model input without raw function identity/address leakage;
- family-grouped or stronger split isolation;
- frozen unseen evaluation sets;
- separate audit metadata from model input;
- explicit provenance for labels and derived evidence.

A gain that disappears on the frozen unseen set does not count as roadmap progress.

### 3.4 Precision before coverage

For automatic `accept` decisions, precision is a hard safety gate. Coverage may increase only while the required precision floor is maintained.

### 3.5 Deterministic retained artifacts

Every major runner must be capable of writing a self-contained evidence directory with:

- input hashes / corpus IDs;
- tool and schema versions;
- command/config manifest;
- predictions/inferences;
- metrics summary;
- failure taxonomy;
- enough raw references to reproduce the decision.

### 3.6 Fail closed

Unsupported evidence shapes, schema mismatches, ambiguous joins, distribution shifts and conflicting high-confidence signals must become explicit `review`/`escalate` states, not guessed answers.

---

# 4. Milestone map

The work is organized into eight engineering milestones. Milestones may overlap, but their exit gates are ordered by dependency.

| Milestone | Priority | Main result | Exit signal |
| --- | --- | --- | --- |
| M0 | P0 | Reproducible baseline and benchmark harness | Clean checkout reproduces canonical metrics/artifacts |
| M1 | P0 | Shared evidence layer consolidation | Generic exporters cover nearly all reusable evidence paths |
| M2 | P0 | Receiver/type/vtable coverage expansion | Major increase in unseen receiver evidence with zero exact-evidence regressions |
| M3 | P0 | Classifier v5.7/v6 | Better `game` precision/recall balance and macro-F1 |
| M4 | P0 | Router v8/v9 | >=0.970 precision and >=0.55 automatic coverage on frozen evaluation |
| M5 | P1 | Semantic inference layer | Auditable behavior/effect summaries and semantic role predictions |
| M6 | P1 | Autonomous orchestration | Iterative evidence acquisition and dependency-aware convergence |
| M7 | P1/P2 | Cross-project validation and productization | Generic pipeline validated outside Nevergone and packaged for reuse |

---

# 5. M0 — Reproducible baseline and benchmark harness

## Goal

Make every later improvement measurable against one canonical, reproducible baseline.

## Work items

### M0.1 Canonical benchmark manifest

Create a versioned benchmark manifest that pins:

- corpus format/version;
- retained corpus IDs and hashes;
- label snapshot;
- family/group assignments;
- train/validation/test/frozen-unseen membership;
- structural evidence versions;
- classifier version/config;
- router version/config;
- expected metric names.

The manifest should identify data, not embed raw proprietary binary content.

### M0.2 One-command benchmark runner

Add one top-level runner that can execute, in dependency order:

1. corpus validation;
2. evidence joins;
3. receiver inference evaluation;
4. classifier evaluation;
5. router evaluation;
6. semantic evaluation when M5 lands;
7. summary generation.

The runner must support a cheap smoke mode and a retained full-evaluation mode.

### M0.3 Machine-readable summary

Standardize a summary schema containing at minimum:

- row counts;
- class distribution;
- coverage;
- precision/recall/F1 per class;
- balanced accuracy;
- macro-F1;
- confusion matrix;
- calibration bins;
- abstention rate;
- receiver evidence coverage;
- exact-evidence mismatch count;
- unresolved/failure taxonomy counts.

### M0.4 Determinism check

Run the same benchmark twice from identical inputs and compare:

- selected rows;
- feature/evidence hashes;
- predictions;
- metrics;
- artifact manifests.

Any nondeterminism must be explicitly justified and bounded.

### M0.5 CI gates

Add lightweight smoke checks for every schema/runner and a scheduled or manually dispatched heavier frozen benchmark.

## M0 exit criteria

- A clean checkout can reproduce the canonical smoke benchmark without manual file editing.
- Retained full benchmark emits all expected artifacts and metrics.
- Dataset/split identity is present in every reported ML metric bundle.
- Determinism check passes for deterministic stages.
- Schema/version mismatch fails closed.

---

# 6. M1 — Finish `revenginstr` evidence consolidation

## Goal

Remove Nevergone-specific duplication from generic structural evidence extraction and establish stable shared schemas.

Recent work already routes multiple exporters through shared implementations, including P-code, defined strings, address references, function operands, address metadata, indirect-call evidence, program indexes, field-store evidence and parameter-store evidence. M1 finishes the consolidation and makes parity measurable.

## Work items

### M1.1 Exporter inventory

Create an inventory of every analysis exporter under `tools/ghidra/`, `tools/ml/` and related runners, classified as:

- shared and canonical;
- shared with Nevergone compatibility wrapper;
- project-specific by design;
- duplicate pending migration;
- obsolete pending removal.

### M1.2 Stable evidence schemas

For each shared evidence family define:

- schema name/version;
- required fields;
- optional fields;
- normalization rules;
- provenance fields;
- ordering guarantees;
- failure behavior.

### M1.3 Parity fixtures

For every migrated exporter, retain small legal fixtures or synthetic projects that permit parity testing without depending on redistributable game binaries.

### M1.4 Compatibility boundary

Keep Nevergone wrappers only where they provide one of:

- project-local path discovery;
- joins to existing retained artifacts;
- compatibility with old retained command lines;
- project-specific postprocessing.

Generic parsing/extraction logic should not remain duplicated in wrappers.

### M1.5 Performance baseline

Record wall time, peak memory and artifact size for representative exports. Avoid regressions that make whole-program re-analysis impractical.

## M1 exit criteria

- >=95% of reusable evidence extraction used by the current analysis pipeline is produced by shared `revenginstr` primitives or thin compatibility wrappers.
- Every migrated exporter has a parity smoke.
- No unexplained schema drift between project-local and shared output.
- All generic evidence types have explicit versions and provenance.

---

# 7. M2 — Receiver/type/vtable recovery coverage

## Goal

Keep the current strong correctness of exact receiver/vtable evidence while substantially increasing the fraction of previously unseen indirect calls for which useful receiver evidence can be recovered.

## Work items

### M2.1 Residual failure taxonomy

Produce a canonical taxonomy for every unresolved receiver query. Suggested top-level categories:

- missing root provenance;
- ambiguous PHI;
- parameter-derived object;
- caller-argument propagation gap;
- field-load/store chain gap;
- singleton/default-instance gap;
- factory return gap;
- container/protobuf/nested object gap;
- mixed receiver candidates;
- missing vtable catalog entry;
- slot ambiguity;
- non-virtual `CALLIND`;
- linker/PLT/veneer artifact;
- decompiler/P-code limitation;
- unsupported pattern.

Every unresolved row must land in exactly one primary category and may have secondary tags.

### M2.2 Provenance graph unification

Represent receiver derivation as a graph rather than a growing list of special-case scripts. Nodes should cover:

- parameters;
- stack/local values;
- field loads;
- field stores;
- returns;
- call arguments;
- PHI/MULTIEQUAL merges;
- globals/GOT cells;
- constructor/factory results;
- singleton/default instances;
- vtable writes and candidate class anchors.

Edges must carry evidence type and confidence/provenance.

### M2.3 Interprocedural propagation

Extend bounded caller/callee propagation with strict cycle and explosion controls. Prioritize:

1. direct caller-argument provenance;
2. factory-return provenance;
3. getter/accessor provenance;
4. field-store to later field-load flow;
5. known constructor/vtable anchors.

### M2.4 Ambiguous PHI handling

Distinguish:

- converged same-type candidates;
- subtype-compatible candidates;
- mixed candidates that require quarantine;
- partially unknown candidates.

Do not collapse mixed candidates to a single type solely to increase coverage.

### M2.5 Receiver candidate scoring

Score candidates from independent evidence dimensions rather than one opaque score. Example dimensions:

- exact vtable witness;
- constructor/factory witness;
- field-offset compatibility;
- slot compatibility;
- caller provenance;
- callgraph consistency;
- object-size/layout compatibility;
- runtime witness.

### M2.6 Unseen receiver benchmark

Freeze at least one receiver benchmark whose functions/structural families were not used to write the recovery rule being tested.

## M2 target metrics

Near-term target:

- increase totally-unseen receiver evidence coverage from the current low-20% range to **>=35%**;
- preserve **zero mismatch** on exact retained vtable-slot/runtime fixtures;
- reduce `unsupported/unknown` residuals by at least **30% relative** without converting them into false confident resolutions.

Stretch target:

- **>=50%** useful receiver evidence coverage on the same frozen unseen benchmark;
- no material increase in mixed-receiver false convergence.

## M2 exit criteria

- Exact evidence mismatch count remains zero on all retained hard gates.
- Frozen unseen receiver evidence coverage >=35%.
- Every unresolved query has an auditable failure category.
- New recovery rules show positive frozen-set gain, not only in-sample gain.

---

# 8. M3 — Production classifier v5.7/v6

## Goal

Improve the `game` vs `engine` decision boundary while preserving leakage resistance and calibration.

The problem is no longer simply model capacity. The roadmap should treat the classifier as an evidence-aware selective predictor with explicit uncertainty.

## Work items

### M3.1 Threshold/calibration sweep

Evaluate per-class thresholds over frozen family-grouped folds and unseen sets. Record full precision/recall/coverage curves rather than selecting a threshold from one headline metric.

### M3.2 Error-bucket analysis

For every major confusion pair, especially `engine -> game` and `game -> engine`, cluster errors by structural/evidence attributes:

- callgraph neighborhood;
- receiver evidence availability;
- namespace/symbol evidence quality where available only for audit;
- function size/complexity bucket;
- indirect-call density;
- object-field behavior;
- external/API call profile;
- family membership;
- truncation status.

Use audit-only identity data to understand errors, never as a hidden model feature.

### M3.3 Evidence ablations

Measure marginal value of:

- normalized P-code text;
- structural buckets;
- callgraph features;
- receiver/type features;
- field-access features;
- external-call categories;
- neighborhood predictions;
- family priors where legally/evaluationally safe.

No feature family should be retained solely because aggregate in-sample accuracy rises.

### M3.4 Effect features

Introduce model-safe coarse behavior features derived from the future semantic/effect IR, for example:

- object reads/writes;
- allocation/deallocation behavior;
- registration/callback patterns;
- parser-like loops;
- render/audio/network/file API categories;
- state-machine-like branch/update structure.

Do not inject raw strings, addresses or project-specific class identities into the generic model input unless a separate explicitly non-transferable experiment is being run.

### M3.5 Calibration

Add reliability diagrams / expected calibration error and class-specific calibration. Confidence must be meaningful enough for the router to consume.

### M3.6 Hard negative mining

Create retained hard-negative sets from the recurring `engine -> game` false-positive families. Keep them out of threshold selection when they are used as a final holdout.

## M3 target metrics

Primary near-term goals on frozen family-grouped evaluation:

- macro-F1 **>=0.80**;
- `game` precision **>=0.70**;
- `game` recall **>=0.65**;
- `engine -> game` errors below the current conservative baseline while meeting the recall floor;
- no regression greater than 2 percentage points in other major classes without documented tradeoff.

Stretch goals:

- macro-F1 **>=0.83**;
- `game` precision **>=0.75**;
- `game` recall **>=0.70**.

## M3 exit criteria

- The selected operating point is chosen from frozen validation policy and confirmed on an untouched holdout.
- `game` precision and recall both meet the near-term floors.
- Model confidence is calibrated sufficiently for selective routing.
- Reproduction script regenerates predictions and summary from pinned inputs.

---

# 9. M4 — Selective router v8/v9

## Goal

Automate substantially more decisions while preserving the current high precision of accepted predictions.

The router should decide whether a conclusion is safe to consume automatically, not merely whether the classifier has a high softmax/logit confidence.

## Work items

### M4.1 Evidence-aware routing

Use independent signals including:

- calibrated classifier confidence;
- classifier margin/entropy;
- OOD score;
- family distance;
- receiver evidence quality;
- callgraph support;
- cross-model agreement;
- evidence conflicts;
- known failure bucket membership.

### M4.2 Per-class routing policy

Use distinct acceptance thresholds for `game`, `engine`, `media_library`, `runtime_library` rather than forcing one global threshold.

### M4.3 Hierarchical decision policy

Allow the router to safely accept a coarse answer while escalating a finer one. Example:

- accept `code_domain=game`;
- review `subsystem=combat`;
- escalate exact semantic role.

### M4.4 Conflict quarantine

Any contradiction between high-confidence structural evidence and model prediction must force `review` or `escalate` until explicitly resolved.

### M4.5 Coverage decomposition

Report coverage separately for:

- all rows;
- in-distribution families;
- distant/OOD families;
- rows with receiver evidence;
- rows without receiver evidence;
- each predicted class.

This prevents aggregate coverage from hiding weak regions.

### M4.6 Frozen routing gate

Freeze one untouched routing holdout. Thresholds may not be tuned on this set.

## M4 target metrics

Hard gate:

- accepted-decision precision **>=0.970**;
- coverage **>=0.55**.

Stretch gate:

- precision **>=0.975**;
- coverage **>=0.65**.

Additional safety goals:

- critical false-accept rate <1%;
- all detected evidence conflicts routed away from `accept`;
- no subgroup with enough samples may silently fall far below the global precision floor.

## M4 exit criteria

- >=0.970 precision and >=0.55 coverage on the frozen routing holdout.
- Router emits explicit reason codes for `accept`, `review` and `escalate`.
- Calibration/OOD/conflict inputs are persisted in the decision artifact.
- Threshold/config version is pinned and reproducible.

---

# 10. M5 — Semantic inference layer

## Goal

Move from "what domain is this function in?" to "what observable behavior does this function implement?" without losing auditability.

This is the largest remaining capability gap.

## 10.1 Build a semantic Effect IR

For every function, emit a normalized effect summary independent of source names where possible.

Suggested fields:

### Memory/object behavior

- reads parameter-derived fields;
- writes parameter-derived fields;
- reads/writes globals;
- candidate field offsets and widths;
- copies/clears blocks;
- allocation/deallocation;
- ownership/refcount patterns.

### Call behavior

- internal/external calls;
- direct/indirect calls;
- API category calls;
- callbacks registered/invoked;
- factory/getter/setter-like patterns;
- caller/callee neighborhood summaries.

### Control behavior

- loop structure;
- dominant branches;
- switch/state-machine patterns;
- early validation/fail paths;
- retry/poll/update patterns.

### Data behavior

- parser/decoder-like sequential reads;
- serializer-like sequential writes;
- hashing/checksum/comparison patterns;
- table/index lookup patterns;
- coordinate/math-heavy transforms;
- container iteration.

### Object/type context

- receiver candidates;
- candidate vtable/slot evidence;
- constructor/destructor relationship;
- object-layout compatibility;
- neighboring methods with shared receiver evidence.

Every Effect IR field must preserve the evidence source used to infer it.

## 10.2 Semantic role taxonomy

Start with a deliberately bounded taxonomy. Example first-level roles:

- lifecycle/init/shutdown;
- getter/setter/accessor;
- factory/constructor/destructor;
- parser/decoder/loader;
- serializer/save writer;
- state update/tick;
- input handler;
- renderer/render submission;
- animation/action update;
- physics/collision query;
- combat/damage/state transition;
- resource lookup/cache;
- callback adapter/scheduler thunk;
- network/media/runtime adapter;
- utility/container/math;
- unknown/compound.

Allow multi-label or compound roles where the evidence requires it. Do not force every function into one semantic class.

## 10.3 Function summaries

Generate structured summaries before natural-language summaries. A structured summary should state claims such as:

- likely receiver/type candidate;
- fields read/written;
- important callees;
- external API categories;
- state transitions;
- probable role;
- uncertainty/conflicts;
- evidence references.

Natural-language explanation is a rendering of this structured object, not the source of truth.

## 10.4 State-machine recovery

Detect state variables from:

- repeated reads + comparisons;
- bounded enum-like constants;
- writes followed by branch-dependent call patterns;
- switch/jump-table structure;
- cross-function field sharing.

Construct candidate state-transition graphs with confidence per edge.

## 10.5 Object-role clustering

Cluster methods sharing:

- receiver evidence;
- field offsets;
- vtable neighborhood;
- callgraph neighborhood;
- Effect IR signatures.

Use this to infer coherent object roles even when individual function symbols are absent.

## 10.6 Semantic labels and evaluation set

Build an evidence-backed semantic evaluation set of at least **500 functions** as the first milestone, growing toward **1,000+** across multiple projects.

Labels should include:

- role(s);
- evidence quality;
- ambiguity flag;
- reviewer notes;
- source evidence link;
- whether the function is safe for automated reconstruction use.

## M5 target metrics

First target:

- role macro-F1 **>=0.75** on a family-isolated semantic holdout;
- >=90% human acceptance for **high-confidence** structured summaries;
- unsupported factual claims in high-confidence summaries <5%;
- every high-confidence semantic claim linked to machine-readable evidence.

Stretch target:

- role macro-F1 >=0.82;
- >=95% human acceptance of high-confidence summaries;
- state-transition recovery validated on at least three independently understood subsystems.

## M5 exit criteria

- Stable Effect IR schema exists.
- At least 500 evidence-backed semantic evaluation functions exist.
- Structured summaries are reproducible and evidence-linked.
- High-confidence semantic role predictions meet the first target metrics.
- Natural-language summaries cannot introduce claims absent from the structured semantic object.

---

# 11. M6 — Autonomous analysis orchestration

## Goal

Turn individual analyzers into an iterative system that identifies uncertainty, requests the next most useful evidence, reruns dependent stages, and stops when evidence converges or escalation is required.

## Work items

### M6.1 Analysis task graph

Represent dependencies explicitly, for example:

`P-code -> callgraph -> receiver provenance -> vtable/slot -> Effect IR -> classifier -> semantic role -> router`.

A changed upstream artifact should invalidate only dependent downstream artifacts.

### M6.2 Evidence request planner

For an unresolved/high-value function, select the cheapest useful next action from a bounded menu such as:

- export a deeper P-code slice;
- inspect callers/callees;
- recover field stores;
- inspect constructor/vtable anchors;
- run a bounded native probe;
- request neighboring family examples;
- defer to human review.

The planner must not invent arbitrary executable probes. Actions are selected from reviewed, bounded templates.

### M6.3 Convergence policy

Define explicit convergence states:

- `accepted_exact`;
- `accepted_high_confidence`;
- `review_required`;
- `escalated_conflict`;
- `unsupported`;
- `budget_exhausted`.

Repeated passes may not endlessly oscillate between hypotheses.

### M6.4 Decision ledger

Persist every important decision with:

- previous state;
- new state;
- evidence added;
- rule/model responsible;
- confidence change;
- artifact references.

### M6.5 Human feedback ingestion

Human review should be able to mark:

- confirmed;
- rejected;
- partially correct;
- ambiguous;
- needs more evidence.

Feedback must not automatically become training truth without provenance and review quality.

### M6.6 Cost-aware prioritization

Prioritize functions by downstream leverage:

- number of unresolved callers/callees;
- number of indirect dispatches unlocked;
- centrality in target gameplay/runtime path;
- uncertainty reduction expected from resolving the function.

## M6 target metrics

First target on supported frozen tasks:

- automatically resolve **>=65%** of supported functions;
- critical wrong-accept rate **<2%**;
- reduce human-review workload by **>=50%** versus classifier-only triage;
- deterministic decision ledger for every automatic resolution.

Stretch target:

- >=80% automatic resolution;
- critical wrong-accept rate <1%;
- clear convergence within a bounded number of analysis passes for >=95% of evaluated functions.

## M6 exit criteria

- Orchestrator can run from raw supported evidence to final decisions without manual intermediate commands.
- Every accepted result is traceable to evidence and versioned logic/model output.
- Conflicts and OOD inputs fail closed.
- Human correction can be replayed and audited.

---

# 12. M7 — Cross-project validation and productization

## Goal

Prove that the analysis system is generic rather than Nevergone-shaped and package it so another project can consume it without copying project-local scripts.

## Work items

### M7.1 Cross-project corpus

Select at least 2 additional native binaries/projects with different characteristics, ideally including variation in:

- compiler/toolchain;
- symbol availability;
- engine/framework;
- optimization level;
- binary size;
- architecture where supported.

Nevergone-specific labels or names must not leak into generic features.

### M7.2 Zero-adaptation run

Run the generic pipeline with no project-specific rules and record:

- evidence extraction success;
- receiver coverage;
- classifier metrics where labels exist;
- router coverage/precision;
- semantic-summary audit quality.

### M7.3 Adapter accounting

Any project-specific adapter must be explicit and classified as:

- path/input adapter;
- schema conversion;
- ABI/architecture adapter;
- evidence source adapter;
- semantic/domain rule.

The number and complexity of semantic/domain rules is a key generalization metric.

### M7.4 Distribution-shift detection

Detect and report major drift in:

- P-code feature distribution;
- function-size buckets;
- external API categories;
- receiver evidence availability;
- classifier confidence/calibration;
- OOD/family distance.

### M7.5 Public/stable interfaces

Define stable entry points for:

- evidence extraction;
- analysis execution;
- result JSON;
- report generation;
- CI benchmark execution.

### M7.6 Documentation

Provide:

- installation guide;
- Ghidra setup;
- minimal first analysis;
- schema reference;
- benchmark guide;
- adding a project adapter;
- interpreting confidence and evidence;
- failure/triage guide.

## M7 exit criteria

- Generic pipeline runs successfully on at least 2 non-Nevergone targets.
- Primary metrics do not collapse without a documented distribution-shift explanation.
- No hidden Nevergone-specific identity features are required for the generic path.
- A new project can produce a first evidence + triage report through documented commands.

---

# 13. Evaluation and CI matrix

Every major change should run the cheapest relevant subset of this matrix.

| Test layer | Purpose | Required for |
| --- | --- | --- |
| Unit tests | Parser/scorer/schema correctness | Every logic change |
| Synthetic Ghidra fixtures | Exact evidence behavior without proprietary inputs | Exporter/recovery changes |
| Exporter parity | Shared vs compatibility output | `revenginstr` migrations |
| Retained Nevergone smoke | Prevent project regressions | Most analysis changes |
| Family-grouped evaluation | Prevent structural duplicate leakage | ML changes |
| Frozen unseen benchmark | Measure real generalization | Classifier/router/receiver changes |
| Exact receiver/vtable fixtures | Safety gate | Receiver changes |
| Calibration/OOD evaluation | Safe selective automation | Classifier/router changes |
| Semantic audit set | Claim factuality and role quality | M5+ |
| Cross-project holdout | Transfer validation | M7 / release |
| Determinism test | Reproducibility | Pipeline releases |
| Performance benchmark | Keep whole-program analysis practical | Exporter/orchestrator changes |

A PR that changes metrics must include the before/after retained summary or explain why the relevant full benchmark cannot run in CI and where its retained artifact is stored.

---

# 14. KPI dashboard

The following table should become the canonical high-level dashboard. Baselines should be refreshed only from pinned retained runs.

| KPI | Current planning baseline | Next gate | Stretch |
| --- | ---: | ---: | ---: |
| Classifier macro-F1 | ~0.765 | >=0.80 | >=0.83 |
| `game` precision | 0.42 broad / ~0.70 conservative operating point | >=0.70 | >=0.75 |
| `game` recall | ~0.71 broad / ~0.52 conservative operating point | >=0.65 | >=0.70 |
| Router accepted precision | ~0.974 | >=0.970 hard floor | >=0.975 |
| Router coverage | ~0.44 current selective / ~0.58 earlier nested | >=0.55 | >=0.65 |
| Exact vtable-slot mismatch | 0 on retained checks | 0 | 0 |
| Totally-unseen receiver evidence coverage | low-20% range | >=35% | >=50% |
| Semantic role macro-F1 | not yet canonical | >=0.75 | >=0.82 |
| High-confidence semantic summary acceptance | not yet canonical | >=90% | >=95% |
| Autonomous supported-task resolution | not yet canonical | >=65% | >=80% |
| Autonomous critical wrong-accept rate | not yet canonical | <2% | <1% |
| Cross-project validated targets | Nevergone testbed | >=2 additional | >=3 additional |

## Metric precedence

If aggregate accuracy improves while accepted-decision precision, exact-evidence correctness, leakage resistance or frozen-unseen performance regresses, the change is **not** considered a roadmap improvement.

---

# 15. Data and label strategy

## 15.1 Separate identities from learnable features

Keep source names, addresses, raw strings and project-local identifiers in trace/audit artifacts unless a particular experiment explicitly studies them. Generic model input must remain normalized.

## 15.2 Evidence quality tiers

Continue using explicit evidence quality such as `gold`, `silver`, `bronze`, but add machine-readable reason codes rather than relying only on free text.

## 15.3 Do not manufacture negatives

Unknown functions are not automatically `other`, `engine`, or non-game. Negative labels require their own evidence.

## 15.4 Active labeling

Prioritize manual evidence work for rows that maximize information gain:

- high-confidence disagreements between analyzers;
- dense error clusters;
- central callgraph functions;
- repeated structural families;
- uncertain functions whose resolution unlocks many receiver/semantic decisions.

## 15.5 Frozen evaluation discipline

When a frozen set materially influences a new recovery rule or threshold, retire it as a final untouched test set and create a new sealed holdout. Do not repeatedly optimize against one "frozen" benchmark until it becomes training-by-proxy.

---

# 16. Suggested implementation sequence — next PR-sized tasks

The following sequence is intentionally concrete. Each item should be small enough to review independently and should retain before/after evidence where applicable.

## P0 — immediate

1. **Canonical analysis benchmark manifest**  
   Add pinned dataset/evidence/config identities and a versioned summary schema.

2. **One-command analysis benchmark runner**  
   Produce receiver + classifier + router summaries from a single retained run directory.

3. **`revenginstr` exporter inventory**  
   Classify remaining project-local Ghidra/analysis exporters and identify duplicates/migration targets.

4. **Receiver residual taxonomy v1**  
   Ensure every unresolved receiver query receives a stable reason code.

5. **Frozen receiver coverage report**  
   Add per-pattern coverage and exact-evidence mismatch gates to the retained unseen benchmark.

6. **Classifier v5.7 threshold/calibration sweep**  
   Emit precision/recall/coverage curves for `game` and `engine`, including the existing conservative operating point.

7. **Classifier error-cluster report**  
   Cluster `engine -> game` and `game -> engine` errors using audit-only structural/evidence metadata.

8. **Router v8 evidence-aware scoring**  
   Add calibrated confidence, OOD/family distance, evidence quality and conflict reason codes.

9. **Router frozen >=0.970 / >=0.55 gate**  
   Make the target an explicit regression test rather than an informal experiment goal.

10. **Analysis metric dashboard artifact**  
    Generate one Markdown/JSON summary with baselines, deltas and gate status.

## P1 — immediately after the P0 baseline is stable

11. **Receiver provenance graph IR**  
    Replace additional special-case handoffs with one graph representation and provenance-bearing edges.

12. **Interprocedural receiver propagation v1**  
    Caller args + factory returns + field stores/loads with strict depth/budget limits.

13. **Semantic Effect IR schema + extractor v1**  
    Start with field effects, calls, allocations, control-shape and API categories.

14. **Semantic gold-set v1 (>=500 functions)**  
    Build auditable role/effect labels without leaking identities into model input.

15. **Structured function summary v1**  
    Render only claims present in Effect IR and attach evidence references.

16. **Semantic role baseline**  
    Establish heuristic and sparse-model baselines before adding heavier models.

17. **State-machine candidate detector**  
    Recover candidate state fields and transition edges with per-edge provenance.

18. **Object-role clustering**  
    Combine receiver, field, vtable, callgraph and Effect IR evidence.

19. **Autonomous task graph / invalidation model**  
    Define stage dependencies and incremental recomputation.

20. **Decision ledger + convergence states**  
    Persist why an analysis conclusion changed and why the system stopped.

## P2 — generalization and scale

21. **Bounded evidence request planner**  
    Select the next reviewed probe/export action from uncertainty and expected leverage.

22. **Human review feedback format**  
    Replayable confirmed/rejected/ambiguous judgments with provenance.

23. **Cross-project holdout harness**  
    Run the same generic pipeline and KPI summary on additional binaries.

24. **Distribution-shift report**  
    Measure feature/evidence/calibration drift before trusting cross-project predictions.

25. **Stable analysis CLI/result schema**  
    Expose the generic pipeline as a reusable product rather than a collection of project-local runners.

---

# 17. Recommended milestone execution order

## Wave A — measurement and safety

Complete M0 plus the highest-value M1 cleanup. Do not add major semantic complexity until the benchmark harness can prove whether it helped.

Deliverables:

- canonical benchmark manifest;
- one-command runner;
- dashboard artifact;
- exporter inventory/parity gates.

## Wave B — structural coverage

Focus on M2 while classifier experiments continue against the stable benchmark.

Deliverables:

- receiver residual taxonomy;
- provenance graph;
- >=35% frozen unseen receiver coverage;
- zero exact-evidence mismatches.

## Wave C — classifier/router operating point

Finish M3 and M4.

Deliverables:

- classifier v5.7/v6 with balanced `game` precision/recall;
- calibrated confidence;
- router >=0.970 precision and >=0.55 coverage;
- explicit conflict/OOD reason codes.

At the end of Wave C the existing analysis stack should be considered a mature **assisted reverse-engineering system**.

## Wave D — semantic understanding

Implement M5.

Deliverables:

- Effect IR;
- 500+ semantic gold set;
- structured summaries;
- role classifier;
- state-machine/object-role recovery.

This wave is expected to produce the largest capability increase from the current baseline.

## Wave E — autonomy

Implement M6.

Deliverables:

- task graph;
- evidence planner;
- decision ledger;
- bounded convergence;
- >=65% automatic resolution on supported frozen tasks with <2% critical wrong accepts.

## Wave F — transfer and productization

Implement M7.

Deliverables:

- 2+ non-Nevergone validation targets;
- distribution-shift handling;
- stable CLI/schemas;
- project-adapter guide;
- release-grade benchmark/report workflow.

---

# 18. Definition of done for analysis features

An analysis feature is not done merely because it produces plausible output. A feature is complete only when:

1. its input/output schema is explicit;
2. evidence provenance is retained;
3. unsupported inputs fail closed;
4. unit/smoke coverage exists;
5. relevant frozen evaluation shows no unacceptable regression;
6. before/after metrics are retained when behavior changes;
7. deterministic behavior is verified where expected;
8. documentation explains how to reproduce it;
9. generic logic is not unnecessarily coupled to Nevergone;
10. downstream consumers distinguish exact evidence from inferred/model evidence.

---

# 19. Major risks and mitigations

## Risk: overfitting the whole toolchain to Nevergone

**Mitigation:** cross-project frozen tests, generic schemas, audit of project-specific adapters, normalized input and early M7 zero-adaptation runs.

## Risk: metric chasing against one frozen set

**Mitigation:** rotate final holdouts once their error patterns materially influence implementation; retain family-grouped and cross-project tests.

## Risk: coverage pressure causes false accepts

**Mitigation:** hard >=0.970 routing precision floor, explicit OOD/conflict quarantine, subgroup reporting and abstention-first policy.

## Risk: correlated evidence is treated as independent confirmation

**Mitigation:** evidence lineage graph; do not count derived evidence twice; mark common upstream sources.

## Risk: semantic labels are subjective

**Mitigation:** structured roles/effects, ambiguity flags, multiple evidence sources, reviewer agreement sampling and a real `unknown/compound` option.

## Risk: schema migration silently breaks retained evidence

**Mitigation:** versioned schemas, compatibility readers where necessary, parity fixtures and fail-closed version checks.

## Risk: orchestrator amplifies an early wrong hypothesis

**Mitigation:** immutable decision ledger, confidence caps for inferred evidence, conflict checks, exact-evidence precedence and bounded iterative passes.

## Risk: heavier models hide structural weaknesses

**Mitigation:** maintain heuristic/sparse baselines, run feature ablations and require evidence-linked structured outputs before natural-language generation.

---

# 20. Success states

## Assisted-analysis milestone

The system is a mature assisted reverse-engineering stack when all of the following are true:

- exact structural evidence remains regression-free;
- receiver evidence coverage >=35% on frozen unseen data;
- classifier macro-F1 >=0.80 with `game` precision >=0.70 and recall >=0.65;
- selective router precision >=0.970 and coverage >=0.55;
- every accepted decision is reproducible and provenance-linked.

## Semantic-analysis milestone

The system reaches useful semantic-analysis maturity when:

- Effect IR is stable;
- semantic gold set >=500 functions;
- role macro-F1 >=0.75;
- >=90% of high-confidence structured summaries are accepted by audit;
- state/object behavior can be recovered across multiple independently understood subsystems.

## Autonomous-analysis milestone

The system reaches the first practical autonomous-analysis milestone when:

- >=65% of supported frozen tasks resolve automatically;
- critical wrong accepts remain <2%;
- unresolved/conflicting cases are reliably escalated rather than guessed;
- the full decision/evidence chain is replayable;
- at least two non-Nevergone projects pass the generic pipeline with measured, documented transfer behavior.

## Long-term target

A project should be able to provide a supported native binary plus an analyzed Ghidra project and receive a reproducible analysis package that automatically resolves most routine structural and semantic questions, isolates genuinely hard cases for human review, and can be consumed by recompilation/decompilation tooling without treating model output as proof.

---

# 21. Immediate next action

Start with **M0.1 + M0.2**: pin the canonical benchmark manifest and add the one-command retained benchmark runner. The current repository already has enough mature analyzers that the highest-leverage improvement is now a unified measurement/control surface. Once that exists, receiver, classifier, router and semantic work can advance against the same objective gates instead of separate experiment-specific summaries.
