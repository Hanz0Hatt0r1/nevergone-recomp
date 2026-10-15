# Tutorial scene-limit evidence

The recovered tutorial enemy geometry consumes three unsigned values from an
object reached through this original pointer path:

```text
GameScene +0x238 -> +0x20 -> raw scene-limit field
```

The current project names the resulting inputs `vertical_edge_a`,
`vertical_edge_b`, and `right_edge_x`, but does not assign those names to raw
object offsets without structural evidence.

This distinction matters because the generic scene-list `GameLevelsData` loader
also retains `gsresfile03` at `+0x20`. The fresh-role tutorial instead enters
through binary `cp_lesson.glData`; equal offsets in those two recovered paths do
not by themselves prove equal object semantics.

## Two-stage probe

The exported symbol corpus gives stable thin wrapper entries for:

```text
GameSceneUI::nextLesson()                 ELF 0x003d67b8
GameSceneUI::nextLesson(MEPLAYERSTATE)    ELF 0x003d67ac
```

The analyzed Ghidra image used by the existing project evidence is based at an
address delta of `+0x10000`, so the default wrapper entries are `003e67b8` and
`003e67ac`.

Run:

```bash
python3 tools/run_tutorial_scene_limit_probe.py \
  --ghidra /opt/ghidra \
  --project-dir /path/to/ghidra/projects \
  --project-name Nevergone \
  --program libcocos2dcpp.so
```

The wrapper deliberately does not guess private `LessonLayer` function
addresses. Instead it:

1. exports high-P-code flow for each known `GameSceneUI::nextLesson*` wrapper;
2. requires exactly one resolved direct callee from each thin wrapper;
3. exports high-P-code flow for those resolved target functions;
4. runs `tutorial_scene_limit_evidence.py` over both targets.

Outputs are written beneath:

```text
build/tutorial-scene-limit-evidence/
```

including `tutorial-scene-limit-report.txt`.

## Machine-readable candidate handoff

Once the text report says `status: three-limit-candidate`, convert the same two
LessonLayer exports into a bounded machine-readable candidate:

```bash
python3 tools/tutorial_scene_limit_candidate.py \
  build/tutorial-scene-limit-evidence/lesson-layer-next-lesson.tsv \
  build/tutorial-scene-limit-evidence/lesson-layer-next-lesson-state.tsv \
  --output build/tutorial-scene-limit-evidence/tutorial-scene-limit-candidate.json
```

The JSON schema is:

```text
NEVERGONE.TutorialSceneLimitCandidate/1
```

The candidate records the exact recovered pointer-prefix offsets, one unique
`right_edge_x_field_offset`, two sorted `vertical_field_offsets`, and the
underlying function/load-site evidence. It deliberately carries:

```text
review_status = candidate
vertical_order_status = unresolved
```

It therefore cannot be used directly as a production `ReviewedSceneLimits`
provider. Incomplete or ambiguous structural evidence fails closed and no
candidate is emitted.

## Structural acceptance rules

The analyzer reconstructs pointer expressions only through bounded,
value-preserving P-code operations and constant pointer arithmetic. A raw field
is considered only when a LOAD path contains the exact dereference prefix:

```text
0x238 -> 0x20 -> field
```

Unrelated uses of the same numeric offsets elsewhere in either function are
ignored.

The recovered geometry gives two independent downstream constants that can
classify candidates without assigning map semantics:

- `500` / `500.0f`: horizontal room test, therefore a `right-edge-x-candidate`;
- `20` / `20.0f`: vertical proximity test, therefore a
  `vertical-edge-candidate`.

A complete structural result requires exactly one right-edge candidate and two
distinct vertical candidates. The report and JSON candidate intentionally leave
the two vertical fields as unordered candidates; the original branch order must
be recovered before mapping them to `vertical_edge_a` versus `vertical_edge_b`.

## Runtime boundary

This probe does not hard-code any scene-limit offset or value into Android
runtime code. After a real analyzed-library run produces a unique result, the
remaining steps are:

1. recover the first/second vertical-branch ordering;
2. promote the candidate only after that ordering is independently reviewed;
3. map the confirmed raw fields into a project-owned tutorial spawn-input
   snapshot;
4. combine them with current reconstructed player position and the proven
   `PLAYERFACEDIR` provider;
5. start the Attack roster only when the reviewed enemy identity/base-HP mapping
   is also available.

Until all of those inputs are explicit, enemy materialization remains
fail-closed rather than substituting guessed boundaries.
