# SingleSelectHero: timed door and reel actions from original ELF

## Provenance

The timings and targets below are derived from the user's focused
`SingleSelectHero::Carousel` and `SingleSelectHero::OpenTheDoor` instruction
and High-P-code exports plus the original ARM32 `libcocos2dcpp.so`.
They are not hand-tuned screenshot approximations.

- Original library SHA-256:
  `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`
- Focused archive SHA-256:
  `504e3c56d5688ea98d63181adffc68cb544303f481c841b4aeb55345b3cf358c`
- Source exports: `004388ac.pcode-flow.tsv`,
  `00437f00.pcode-flow.tsv`, `selection.instructions.jsonl.gz`
  (the last filename carries uncompressed JSONL bytes).
- Ghidra instruction addresses in the recorded export have a +`0x10000`
  shift relative to this ELF's text file offsets. For example the original
  `OpenTheDoor` prologue at exported address `0x00437f00` is at ELF
  file offset `0x00427f00`, verified against instruction bytes.
  This matters when reading literal-pool values, but does not affect
  addresses used to join Ghidra-exported P-code.

The original library's `0x0043818c..0x004381a4` Ghidra literal-pool
addresses resolve to these `float32` values:
`0.2, 0.3, 0.39, 0.6, 0.06, 0.4, 0.1`.
Do not confuse a literal pool value with the action duration until the
specific use-site has been traced.

## Carousel: three-stage reel move

`SingleSelectHero::Carousel` at Ghidra `0x004388ac`:

1. `0x004389ce..0x00438a16`: subtracts the selected control point from
   the first control point; a 271-unit gap comes from the atlas's
   `xrguangai.png` content width.
2. `0x004389d6..0x004389ea`: the first `CCMoveTo` duration is 1.5 seconds
   when the career-distance is at most two, otherwise 3.0 seconds.
3. `0x00438a5c..0x00438a68`: first move uses
   `CCEaseInOut::create(CCMoveTo::create(duration,target),duration)`.
4. `0x00438a6a..0x00438a8c`: the second move goes to `target ± 3`
   over 0.3 seconds, using `CCEaseSineOut`.
   If new career > old career the sign is positive; otherwise negative.
5. `0x00438a92..0x00438abc`: the third move goes back to `target`
   in 0.6 seconds via `CCEaseSineIn`, then triggers its callback.
6. `0x00438ac0..0x00438ac8`: the hero-holder X compensates the
   reel offset.

Near-total = 2.4 seconds; far-total = 3.9 seconds. This matches the
existing `single_select_hero_transition_timeline` gates.

## Close: OpenTheDoor(false, oldCareer)

`0x004381b0..0x00438390` is the branch taken for a false boolean:

- Original `CCMoveTo` of 2.0 seconds, `CCEaseSineIn`,
  approaching the control-point X.
- Second move 0.3 seconds, `CCEaseSineOut`, to point X - 3.
- Third move 0.39 seconds, `CCEaseSineIn`, returning to point X.
- In parallel, `stopAllActions` and `CCFadeIn/Out/To` calls on
  several additional scene nodes. Their specific visual objects are
  **not yet assigned verified identities**.

The lid translation lasts **2.69 seconds**. The host motion model's
start at X=0 assumes that the preceding open action completed and
placed the same lid at that X; interrupted/overlapping actions must be
checked by the original `numberOfRunningActions` guard before use.

## Open: OpenTheDoor(true, currentCareer)

`0x00437f5c..0x00438082` is the branch taken for a true boolean:

| Stage | Duration | Target | Ease |
| --- | ---: | --- | --- |
| Nudge | 0.2 s | `P.x - 8` | sine-in |
| Callback, delay | 0.5 s delay | hold last X | none |
| Opening travel | 2.0 s | `P.x - currentPositionX` | sine-in |
| Bounce | 0.3 s | travel target + 3 | sine-out |
| Settle | 0.39 s | travel target | sine-in |

Total motion/delay length: **3.39 seconds**, ignoring zero-duration
callback execution. Crucially, the opening travel target is not a
hardcoded zero. Zero follows only when `currentPositionX == P.x`
at the time the action sequence is created. The model exposes that
starting X as an explicit argument.

Before scheduling either branch the original function:

- chooses one lid via `CCArray::objectAtIndex(currentCareer - 1)`;
- reads `CCPointArray::getControlPointAtIndex(currentCareer - 1)`;
- checks `CCNode::numberOfRunningActions()` and returns early
  if an action is already running.

## Implementation scope

`single_select_hero_coffin_motion.h` encodes these movement sequences
as pure, deterministic easing functions. The smoke test verifies
the endpoints, signed overshoot, near/far durations, start-position
dependence, and the 35 Hz sampling contract.

The current GLES renderer draws the selected lid during initial
and non-overlapping change transitions using this model. The
`VisualSnapshot` of `single_select_hero_transition_timeline` survives
the interaction-unlock event and expires at the full animation deadline;
its `initial_positions_verified` gate suppresses early-interrupted
sequences where the native `numberOfRunningActions()` guard makes
the lid's starting pose unknown.

**Limit:** only the actively selected moving cover is rendered, not the
full five-cover reel's visibility/clip effects. The action graph
also changes opacity and callbacks on other nodes; those effects are
still not reproduced and require separate original evidence. This is
not yet pixel-perfect animation parity.

The original APK/OBB, saves and raw Ghidra output are not committed.
