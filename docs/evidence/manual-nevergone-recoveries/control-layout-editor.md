# Manual reconstruction: draggable action-control layout

> Address convention: executable addresses from RevEngInstr use an image base
> `0x1000` above the shipped ELF VMA. Subtract `0x1000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.
> Object-field offsets are unchanged.

This reconstruction continues the saved-position recovery in
[`controls-saved-layout.md`](controls-saved-layout.md). Source evidence comes
from the user-owned ARMv7 `libcocos2dcpp.so` analyzed in Ghidra.

## Recovered behavior

`SetControlPosition::ccTouchBegan @ 0x002a91e0` selects a control sprite from
its bounding box, records its current position, and stores a selection tag.
`SetControlPosition::ccTouchMoved @ 0x002a9ef8` uses that tag to move the
selected sprite, keeps it inside the visible dimensions using its sprite
size, and rejects candidate positions that intersect other control sprites.
For the paired skill buttons it moves the shared anchor and preserves the
recovered A/C offsets. `ToolsQuickUseUI::setHandleActived @ 0x00465d98` loads
`toolsBtn1X/Y` and `toolsBtn2X/Y` and sets both quick-use menu-item positions.
The default positions are the visible-size point. The function uses the shared
`controls_settings` flag. `SetControlPosition::savePositionData @ 0x002aaba4`
stores that flag plus ten float values, applies the three gameplay-control
points to `Controls`, updates `ToolsQuickUseUI`, and closes the editor.
`SetControlPosition::resetAllBtnPosition @ 0x002aacd8` restores the five
positions captured when the editor layer was initialized, then repositions
the preview sprites. Those initial preview points come from the visible-size
point when there is no saved layout, or from saved positions when one exists.

## Reconstructed subset

The Android developer-tools panel can now enter edit mode on the game surface.
Dragging moves the joystick, B, paired A/C, or either quick-use slot. Action
controls and quick-use slots stay within the 1136x640 design area using
imported sprite dimensions. A/C continue to share their original anchor. The
compositor displays the original `stage_use_a.png` and `stage_use_b.png` slot
sprites. Saving writes all ten coordinates in the original preference-key
format. Leaving the activity also saves an active edit. Edits use a small
override file beside the imported preference XML so the source import and its
unrelated preferences remain intact.

The panel's **Restore shipped defaults** action resets the recovered
`Controls::init()` positions and saves that layout. This is an app-level reset
to shipped controls; the original editor reset restores its captured preview
positions.

This implementation restores control placement, not the quick-use item's
inventory icons, cooldowns, or activation actions. The original collision
rejection and editor decorations are also outstanding. The source-function
label describes the directly observed function role, not complete behavioral
parity.

## Corpus label

[`control-layout-editor-labels.jsonl`](control-layout-editor-labels.jsonl)
adds gold `function_role` labels for `ccTouchMoved`,
`ToolsQuickUseUI::setHandleActived`, and `resetAllBtnPosition`. Ghidra's
recovered functions directly show sprite selection state, bounded position
updates, intersection checks, preference reads, and Cocos sprite-position
updates.
