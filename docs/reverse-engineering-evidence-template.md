# Reverse-engineering evidence template

Use this template for behavior-sensitive reconstruction work. The goal is to keep implementation claims traceable to evidence and to make unresolved boundaries explicit.

## Scope

**Subsystem / function:**

**Playable-path checkpoint:** P?

**Tracking issue / PR:**

## Original evidence

**Original symbol(s):**

**Address / offset / binary:**

**Resource path(s) or format:**

**Callers / callees:**

**Observed control flow:**

**Recovered constants / field widths / ordering:**

## Confidence

Choose one:

- **High** - directly established by symbols, decompilation, binary layout, deterministic runtime observation, or multiple independent sources.
- **Medium** - strongly implied by call order/data relationships but one semantic detail remains indirect.
- **Low** - working hypothesis used only to structure the next experiment; must not silently become a compatibility contract.

**Confidence:**

**Why:**

## Clean-room interpretation

Describe the behavior the project actually needs to reproduce. Avoid copying ABI/layout details that are not required by the reachable offline path.

## Unknown boundary

State the first unresolved item precisely. Examples:

- width of the next binary field is not established;
- ownership/lifetime of the returned object is unknown;
- one branch condition is not yet tied to a semantic state;
- asset lookup order is not yet proven.

Do not parse or implement past this boundary merely because plausible data follows it.

## Project-owned implementation

**Files/classes/functions changed:**

**Behavior added:**

**Fallback behavior:**

## Regression coverage

Prefer synthetic fixtures and host-side tests that contain no proprietary bytes.

**Tests:**

**Assertions that protect the evidence boundary:**

## Runtime validation

**Host:**

**Android/emulator/device:**

**Imported user-owned data used:** yes / no

**Observable result:**

## Remaining gap

Name the single next evidence-backed question that most directly advances the playable path.
