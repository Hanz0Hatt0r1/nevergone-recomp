# Manual reconstruction: tutorial lesson prompt

This recovery targets the first missing in-game tutorial panel after the
post-lesson dialogue. The source is the user-owned Nevergone ARMv7 library
`libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Source function and evidence

Ghidra 12.1.2 identifies `LessonLayer::showDialogue()` at `003f12a8` (3,282
bytes). The Nevergone ML corpus record for the same entry reports 7,842 P-code
operations, emits the 256-operation cap, and marks the row truncated. That made
it a strong manual-recovery target: it is a game-side function with a large
surface, but its imported prompt data, asset references, and matching runtime
frame provide a bounded behavior to reconstruct. Its pseudocode establishes
that the current `LessonData` supplies
`getTitle()` and `getConText()`, and that lesson counters below 3 have no
interactive panel. The function creates the imported assets
`lessonsUI/lesson_board.png` and `lessonsUI/lesson_title.png`, with additional
lesson-control sprites, then creates title and body labels in Arial. The ARM
constants passed to the label constructors are 25 for the title and 20 for the
body; the body width is 90% of the board width. The board/title placement uses
visible-size offsets 150 and 60.

The original device capture at `/tmp/nevergone-original-dialogue-1.png`
(SHA-256 `8310d63d8fe43c98be2bcb56e56a39e636cf79578c4a79c7cc5043e13b69078b`)
shows the Run title and instruction in the upper-right area of the live
`cp_lesson` scene. That capture is evidence only and is not committed.

The user-imported `lessonsUI/lesson_text_01.plist` stores each prompt as
`dialogue_text_NN` with `text_title` and `text_01` localization keys. The
interactive runtime counter maps to the original record as
`original_step = tutorial_step + 1`; the first Run prompt therefore reads
`dialogue_text_03`. The imported login CSV resolves those keys for the current
locale. Neither the localized strings nor original image bytes are checked in.

## Reconstructed behavior

`TutorialLessonOverlayAssetLoader.reloadPromptFromFilesDir()` now selects the
record for the active tutorial step, resolves the two localization keys,
rasterizes the labels, and uploads them with the imported board and titlebar.
The GLES layer draws the panel only in `tutorial_session_state::Phase::kInteractive`
and only when the uploaded prompt step matches `tutorial_gameplay_state`.
Consequently prompt text advances with the existing Run, Jump, combat, and
execution state transitions instead of a separate timer or raw button press.

The board, titlebar, and localized text are now present. The static control
diagrams referenced by `showDialogue()` (`lesson_stick.png`, `A1.png`, `B1.png`,
`C1.png`, and `S1_a.png`) are not drawn yet. A first reconstruction of
`LessonLayer::callback1()`'s interactive control cues is documented separately.
The separate player status HUD is also not reconstructed by this change.

## Corpus label

[`tutorial-lesson-prompt-labels.jsonl`](tutorial-lesson-prompt-labels.jsonl)
adds a gold `function_role` label for the manually recovered renderer. The
function's recovered name alone does not establish this role; the label is
based on the focused pseudocode, imported-resource references, and matching
runtime frame above. Merging it with the existing library labels and the six
manual random-mission labels rebuilt the local
`dataset-manual-restoration-v2`: 9,119 labeled functions, including one
`ui_renderer`. This adds supervision; no model was trained or replaced.
