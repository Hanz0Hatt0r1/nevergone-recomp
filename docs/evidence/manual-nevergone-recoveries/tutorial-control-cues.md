# Manual reconstruction: interactive tutorial control cues

This recovery continues the lesson renderer documented in
[`tutorial-lesson-prompt.md`](tutorial-lesson-prompt.md). It uses the user-owned
Never Gone 1.0.9 ARMv7 library `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Function evidence and corpus selection

Ghidra identifies `LessonLayer::callback1(CCObject*)` at `003f0368`. The
Nevergone ML corpus row for that entry records 13,918 P-code operations,
emits the 256-operation cap, and marks the decompilation truncated. It is
therefore a strong continuation target after the prompt renderer: this one
callback stages the interactive hand and per-step button cues.

Focused Ghidra references in the callback resolve these imported resources:

```text
lessonsUI/hand_ring.png
lessonsUI/hand_point%02d.png
lessonsUI/lesson_touch.png
lessonsUI/C2.png
lessonsUI/A2.png
lessonsUI/B2.png
lessonsUI/S2.png
lessonsUI/s_a_to_j_tip.png
```

The decompilation branches on the retained lesson counter, creating C2, A2,
B2, then A2 highlights for the successive Jump, Attack, Block, and
Counter-attack prompts. The following branches add touch plus multi-control
guides, an S2 cue, and the Execution swipe cue. The imported asset tree on the
user's device contains every listed sprite. None of those image bytes are
committed.

## Reconstructed cues

The lesson state machine already exposes the current step, and the Android
action-control geometry mirrors the recovered A/B/C centers. The loader now
composes a transparent, full-design-space cue layer from the original imported
sprites and uploads it with each localized prompt:

- Run: hand ring, touch marker, and hand frame over the joystick center.
- Jump, Attack, Block, and Counter-attack: the matching C2/A2/B2/A2 marker and
  hand cue at the corresponding game control.
- Dodge: the joystick cue and B2 marker together.
- Air Attack: C2 and A2 markers together.
- Execution: A2/C2 endpoints and the original `s_a_to_j_tip.png` swipe cue
  between them.

The drawing order places this transparent layer above the gameplay controls.
The imported first hand frame is currently static; the original repeat timing
and frame alternation remain to be reproduced. The `S2.png` item-slot target
is confirmed in the callback's branch, but this code does not place it because
the standalone action-control geometry has no recovered item-slot center yet.

The implementation uses only user-imported files at runtime. It does not
embed guessed localized text or proprietary sprite data in the repository.

## Corpus label

[`tutorial-control-cues-labels.jsonl`](tutorial-control-cues-labels.jsonl)
adds a gold `function_role` label for this manually reconstructed UI callback.
Its renderer role is supported by the imported sprite references, Cocos sprite
and animation calls, step branches, and the recovered runtime control targets.
Adding this row to the previous manual-restoration labels rebuilt the local
`dataset-manual-restoration-v3`: 9,120 labeled functions and 6,833 structural
groups, including a second `ui_renderer` example. No model was trained or
replaced.
