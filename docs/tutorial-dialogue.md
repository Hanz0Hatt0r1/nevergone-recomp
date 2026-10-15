# Fresh-role guide dialogue boundary

This note records the clean-room dialogue contract immediately after the initial
`LessonLayer::showKeyboardPic()` presentation. Original binary bytes, imported
artwork and localized dialogue strings are not committed.

## Original callback chain

The shipped ARMv7 runtime establishes the following order:

```text
LessonLayer::menuContinueCallback()
  -> firstDlgCallBack(1.0f)
  -> GameSceneUI::showDialogueUI("Guide/guide_text.plist", 8, ..., tag=1)
  -> LessonLayer::dialogEndCallBack(1)
  -> LessonLayer::showDialogue()
  -> interactive lesson callbacks / nextLesson(...)
  -> lesson step 12 + BattleManager::enemyNullCount()==0
  -> scheduleOnce(secondDlgCallBack, 2.0f)
  -> GameSceneUI::showDialogueUI("Guide/guide_text.plist", 9, ..., tag=2)
  -> LessonLayer::dialogEndCallBack(2)
  -> final tutorial continuation
```

`dialogEndCallBack(1)` is a direct tail-call into `showDialogue()`. It does not
leave the whole lesson interlude behind a modal DialogueUI. `showDialogue()`
selectively exposes the joystick/action controls for the recovered lesson
steps, so completion of guide dialogue 8 is the evidence-backed boundary where
the coarse preservation session must enter its interactive tutorial phase.

Dialogue 9 is a separate late boundary. The original `nextLesson(MEPLAYERSTATE)`
checks lesson step `12`; only when `BattleManager::enemyNullCount()` returns zero
does it schedule `secondDlgCallBack(float)` through `CCNode::scheduleOnce()`.
The recovered delay argument is exactly `2.0f`. This must not be replaced by an
immediate post-dialogue-8 transition or a fabricated generic timer.

## Imported guide plist

The user's original `Guide/guide_text.plist` confirms:

```text
auto_play = true
fadein_time = 0.3
fadeout_time = 0.3
dialogue_text_num = 10
```

Dialogue 8 contains three entries and uses localization keys `FGSub1`, `FGSub2`
and `FGSub3`. Their speakers/positions alternate:

```text
line 1: npc 7, npcPos 1, appear 1.0, show delay 4.6
line 2: npc 1, npcPos 0, appear 0.7, show delay 3.0
line 3: npc 7, npcPos 1, appear 1.0, show delay 4.2
```

All three lines request fade-in and fade-out. The runtime resolves the actual
localized text from the user-imported localization table; no translated string
is embedded in project source.

Dialogue 9 contains six later entries (`FGSub4` through `FGSub9`). It remains a
separate reconstruction boundary because the interactive `LessonLayer` sequence
executes between ID 8 and ID 9.

## GameConversation resource and layout contract

`GameConversation::loadDialogueCongiFile()` reads the root keys:

```text
dialogue_text_num
auto_play
fadein_time
fadeout_time
dialogue_text_%02d
```

Each selected dialogue dictionary reads:

```text
text_num
text_%02d_npc
text_%02d_count
text_%02d
appearTime%02d
showDelayTime%02d
npcPos%02d
fadein_effect%02d
fadeout_effect%02d
```

`GameConversation::nextText()` splits encoded speaker IDs into a variant and a
role. For the fresh-role ID-8 sequence only two portrait cases are required:

- role 1: current player, `Dialogue_UI/hero_%02d.png`, current role name;
- role 7: `Dialogue_UI/role_07.png`, name key `NPCName3`.

The recovered npc-position switch places `npcPos=0` at the right edge and
`npcPos=1` at the left edge. The left-side name/content anchors are `(80,180)`
and `(80,150)` in the original design coordinate system. The dialogue
background is `Dialogue_UI/dialogue_bg.png`, centered and horizontally scaled
to the visible width. Name/content labels use Arial 23/20; content is bounded to
450 design pixels.

## Reconstructed ID-8 runtime

`tutorial_dialogue_runtime` begins only after the recovered LessonOverlay
Continue callback has moved the coarse session into `Dialogue`. The Android
loader:

1. parses the user's imported `Guide/guide_text.plist` with the existing safe
   plist reader;
2. requires dialogue ID 8 and `auto_play=true`;
3. resolves each single text key through imported `Login/ALL_Loin.csv`;
4. loads `Dialogue_UI/dialogue_bg.png`, the current player portrait, and
   `Dialogue_UI/role_07.png` from the imported tree;
5. rasterizes the imported localized speaker/content labels without embedding
   original text in the repository;
6. transactionally uploads all three lines before the dialogue is eligible to
   advance.

Autoplay uses the project fixed 35 Hz game clock and each imported line's
`appearTime + showDelayTime`. Root fade-in/fade-out values are applied to lines
whose imported flags request them. The first functional implementation renders
complete localized line text rather than reproducing the original progressive
typewriter reveal; exact glyph-by-glyph timing remains a visual-fidelity task.

After the third line the dialogue runtime retains `lesson-interlude` as its
fine-grained stage, but the Android frame integration immediately publishes the
existing `tutorial_session_state::on_dialogue_complete()` transition. That
moves the coarse session from `Dialogue` to `Interactive`, advances
`tutorial_gameplay_state` from `Dialogue` to `Run`, and unblocks the recovered
gameplay controls on the following frame. This mirrors the original
`dialogEndCallBack(1) -> showDialogue()` handoff without inventing a delay.

The later guide dialogue 9 is intentionally not started by this handoff. Its
known trigger is the recovered step-12 / zero-enemy condition plus the exact
2.0-second `scheduleOnce(secondDlgCallBack, ...)` delay. Wiring that terminal
modal dialogue belongs with the remaining combat/tutorial outcome integration.
Runtime diagnostics continue to expose the dialogue stage, line index, asset
readiness, timing and successful frame counters so real-device evidence can
identify the next boundary explicitly.
