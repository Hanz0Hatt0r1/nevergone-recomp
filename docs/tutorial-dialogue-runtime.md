# Fresh-role tutorial DialogueUI runtime

## Scope

This note records the clean-room reconstruction of the modal dialogue entered by
the fresh-role `LessonLayer` immediately after the initial keyboard lesson's
**Continue** callback. Original binary bytes, plist contents and artwork are not
stored in the repository.

The earlier runtime already preserved the coarse state boundary:

```text
Loading -> LessonOverlay -> Dialogue -> Interactive
```

but `Dialogue` had no renderer or input driver. As a result, a real device could
advance past the reconstructed LoadingLayer and LessonOverlay only to stop on an
invisible modal phase with all gameplay input deliberately blocked.

## Original callback identity

Static recovery from the user's original ARMv7 library establishes the exact
fresh-role callback chain:

```text
LessonLayer::menuContinueCallback
    -> LessonLayer::firstDlgCallBack(float)
    -> GameSceneUI::showDialogueUI(
           "Guide/guide_text.plist",
           8,
           LessonLayer as DialogOverCallBack,
           callback tag 1,
           {0,0,0,10})
```

`LessonLayer::dialogEndCallBack(1)` returns to `LessonLayer::showDialogue()`,
which is the instruction/gameplay side of the tutorial. A separate
`secondDlgCallBack(float)` later opens the same guide file with dialogue id `9`
and callback tag `2`; that terminal tutorial dialogue is therefore **not** folded
into the startup dialogue reconstructed here.

`DialogueUI::ccTouchBegan()` accepts the complete touch surface. For a manual
conversation, `DialogueUI::ccTouchEnded()` forwards to
`GameConversation::nextText()`. When the final page is consumed,
`GameConversation::endConversation()` invokes the registered callback tag.

## Imported config contract

`GameConversation::loadDialogueCongiFile()` reads the user-owned plist using the
following recovered schema:

```text
dialogue_text_num
auto_play
fadein_time
fadeout_time
dialogue_text_%02d
  text_num
  text_%02d_npc
  fadein_effect%02d
  fadeout_effect%02d
  text_%02d_count
  text_%02d
  appearTime%02d
  showDelayTime%02d
  npcPos%02d
```

The `text_%02d` value is passed through the same
`ManagementLayer::GetPlistString()` localization path used elsewhere by the
original game. The clean-room loader therefore resolves each imported group-8
text key through the existing bounded localization CSV implementation instead
of committing or hard-coding dialogue text.

## Recovered baseline presentation

The original `GameConversation` creates `Dialogue_UI/dialogue_bg.png`, centers
it on the visible surface, and scales it by `visible_width / background_width`.
The conversation text uses Arial at 20 design pixels with a 450-pixel width,
left/top anchoring, and the recovered ordinary text origin `(80,150)` in the
1136x640 design space.

`tutorial_dialogue_overlay` reproduces that evidence-backed baseline:

- the background is decoded only from the user's imported asset tree;
- group `8` is required explicitly and bounded to at most 64 pages;
- every page text key must resolve through the imported localization data;
- page text is rasterized as Arial 20 in a 450-pixel layout;
- the imported background and current page are staged to project-owned GLES2
  textures;
- the whole surface is modal, matching the original targeted touch delegate;
- a complete manual touch advances one page;
- the final page publishes `tutorial_session_state::on_dialogue_complete()`,
  entering the already reconstructed interactive tutorial phase.

The renderer and input driver fail closed when imported files, group 8, page
counts, localization keys, decoded pixels or GLES resources are unavailable.
Runtime diagnostics expose the dialogue id, page count/current page, staging and
draw state, touch/page counters and completion count.

## Deliberately remaining fidelity

This change does not invent behavior that has not yet been carried far enough
into the clean-room runtime. In particular:

- the original per-character/typewriter progression is not yet reproduced;
- NPC/player portrait and speaker-name presentation is not yet reconstructed;
- per-page fade-in/fade-out effects are parsed by the original game but not yet
  reproduced by this baseline compositor;
- an imported `auto_play=true` sequence is drawn and reported, but manual touch
  does not override it with a fabricated timer;
- the later guide dialogue id `9` remains attached to the terminal tutorial
  callback and should be reconstructed at that actual lifecycle boundary.

Those are visual/timing fidelity boundaries. The fresh-role startup no longer
needs to remain stuck in an invisible `Dialogue` phase when imported group 8 is
a manual conversation.
