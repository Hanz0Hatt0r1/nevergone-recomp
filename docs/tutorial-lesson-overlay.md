# Fresh-role LessonLayer overlay evidence

This note records the first visible tutorial UI immediately after the recovered
`cp_lesson.glData -> LoadingLayer -> GameScene` handoff. It is derived from the
user-owned Never Gone 1.0.9 ARMv7 binary and the already recovered tutorial
lifecycle. No original asset bytes or screenshots are committed.

## Creation boundary

For game mode `0x0b`, the first ready `GameScene::tick(float)` calls
`GameSceneUI::showLesson()` after the original NPC/enemy initialization gate.
`showLesson()` is idempotent: when no existing lesson object is retained it
calls:

```text
Lesson::createLessonUI(1, 1)
```

stores the returned object in `GameSceneUI`, and adds it as a child with tag
`0x15`.

`Lesson::initLessonUI(1,1)` creates a `LessonLayer` and calls:

```text
LessonLayer::createLessonLayer(1, 1)
```

The layer is retained as the active lesson UI.

## Lesson text source

`LessonLayer::initLessonLayer(1,1)` constructs the resource name:

```text
lessonsUI/lesson_text_01.plist
```

and loads it through `CCDictionary::createWithContentsOfFile()`.

The layer reads per-entry values using the keys:

```text
text_id
text_title
text_01
```

and passes the resulting localization identifiers through the existing
`ManagementLayer::GetPlistString()` path before building visible labels.

The imported plist is not contained in the standalone APK attached to this
project; it belongs to the user-imported game data boundary. The recomp should
therefore parse it from `<files>/assets` at runtime rather than copying text
from the original binary.

## Initial keyboard presentation

After the lesson table is loaded, `initLessonLayer()` immediately calls
`LessonLayer::showKeyboardPic()`.

The original function first calls:

```text
Controls::hideControls()
GameSceneUI::hideToolsUI()
```

This establishes the modal-input/UI boundary already represented by
`tutorial_session_state`: ordinary gameplay controls remain hidden in
`Loading`, `LessonOverlay`, and the post-lesson `Dialogue` phase.

The first imported sprite created by `showKeyboardPic()` is:

```text
lessonsUI/keyboard_lesson.png
```

It is positioned at half the current visible width and half the current visible
height, i.e. centered on the visible surface.

A fixed UI button is then created with the localized key:

```text
Continue
```

Its recovered center is:

```text
x = visible_width - 160
y = 45
```

The button label uses:

```text
font: Arial
size: 25
```

Additional localized labels are attached around the keyboard illustration.
The shipped function references the tutorial/role localization keys including
`RoleState32`, `RoleState33`, `RoleState34` and related entries. Their exact
runtime strings should be taken from the user-imported localization/plist data,
not embedded as guessed English text.

## Continue callback

The fixed Continue button is wired to `LessonLayer::callback1(CCObject*)`.
That callback begins constructing the hand-ring / hand-point tutorial
indicators and then branches on the retained lesson step. Recovered imported
assets used by that path include:

```text
lessonsUI/hand_ring.png
lessonsUI/hand_point%02d.png
lessonsUI/lesson_touch.png
lessonsUI/A2.png
lessonsUI/B2.png
lessonsUI/C2.png
lessonsUI/S2.png
lessonsUI/s_a_to_j_tip.png
```

Those later indicators belong to the interactive tutorial boundary and should
not be displayed before the initial keyboard overview is dismissed.

## Clean-room implementation boundary

The first project-owned `LessonOverlay` compositor should therefore:

1. activate only while `tutorial_session_state::Phase::kLessonOverlay` is live;
2. load `lessonsUI/keyboard_lesson.png` and
   `lessonsUI/lesson_text_01.plist` from the user's imported asset tree;
3. center the keyboard illustration in design/surface space;
4. resolve visible strings through imported localization data;
5. expose the recovered Continue hit target without allowing touches to leak
   to gameplay controls;
6. publish `tutorial_session_state::on_lesson_overlay_complete()` only from the
   reconstructed Continue callback, not from a timer.

The existing tutorial session then enters the separately recovered modal
post-lesson Dialogue phase. Interactive A/B/C/joystick input remains disabled
until that dialogue completes.

## Reconstructed initial overlay

`tutorial_lesson_overlay` now owns the first concrete runtime implementation of
this boundary. `TutorialLessonOverlayAssetLoader` requires both imported
`lessonsUI/keyboard_lesson.png` and `lessonsUI/lesson_text_01.plist`; the plist
must expose the recovered `text_id`, `text_title`, and `text_01` keys before the
overlay is considered stageable. The keyboard bytes remain user-owned and are
decoded locally with `BitmapFactory`.

The Continue label is resolved from the imported `Login/ALL_Loin.csv` through
the same recovered language-column contract used elsewhere in the standalone
UI, then rasterized with Android `Paint`/`Canvas`, `Arial`, size 25. No English
fallback is embedded in the runtime.

The GLES compositor centers the imported keyboard in 1136x640 design space and
places the localized Continue label at the recovered `(visible_width-160, 45)`
center. Until the original button chrome/bounds are recovered, the actionable
region is deliberately limited to the actual localized label rectangle rather
than inventing padding or a button size.

While `LessonOverlay` is active, the post-scene touch bridge sends the complete
surface to this modal handler and swallows all non-Continue touches. A completed
press/release inside the retained label rectangle is the only path that calls
`tutorial_session_state::on_lesson_overlay_complete()`. It therefore advances
to `Dialogue` without a fabricated timer and without exposing joystick/A-B-C
input behind the overlay.

The runtime diagnostics now retain overlay asset readiness, imported pixel
sizes, draw attempts/frames, Continue captures/taps, and successful overlay
completions. The remaining fidelity boundary is the set of additional
localized keyboard labels, original fixed-button chrome/hit area, and the
recovered 0.2-second LoadingLayer fade. The post-lesson Dialogue uses its own
runtime, which hands the scene to the interactive prompts below.

## Interactive lesson prompts

After the post-lesson dialogue completes, the same compositor now draws the
active lesson prompt over the live controls. `TutorialLessonOverlayAssetLoader`
reads the active `dialogue_text_03` through `dialogue_text_11` record from the
user-imported plist, resolves its `text_title` and `text_01` localization keys
through the imported login CSV, and rasterizes the result locally. No tutorial
copy or imported image bytes are stored in the repository.

The compositor uses the imported `lesson_board.png` and `lesson_title.png`
textures and the Arial 25 title / Arial 20 body sizes recovered from
`LessonLayer::showDialogue()`. The panel is positioned using its recovered
visible-size offsets; it is submitted after the game controls and only while
the matching tutorial step is active. This restores the changing Run, Jump,
Attack, Block, and later lesson text panels.

`LessonLayer::callback1()` also creates step-specific control cues. The overlay
now draws the imported touch, ring, hand, and A/B/C highlight sprites at the
recovered joystick and action-button centers. The Execution cue marks the A-to-C
swipe path. The hand currently uses a static frame; the repeat timing and frame
alternation remain to be reproduced. The S2 item-slot cue is confirmed in the
callback, but its exact position is unresolved. The separate player status HUD
remains visible fidelity work.
