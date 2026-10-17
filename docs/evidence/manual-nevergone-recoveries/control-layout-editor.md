# Manual reconstruction: draggable action-control layout

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
recovered A/C offsets. `SetControlPosition::savePositionData @ 0x002aaba4`
stores `controls_settings` plus ten float values, applies the positions to
`Controls`, updates `ToolsQuickUseUI`, and closes the editor.

## Reconstructed subset

The Android developer-tools panel can now enter edit mode on the game surface.
Dragging moves the joystick, B, or the paired A/C controls; action controls
stay within the 1136x640 design area using imported sprite dimensions. A/C
continue to share their original anchor. Saving writes the corresponding six
coordinates in the same preference-key format consumed by `Controls::init()`.
Leaving the activity also saves an active edit. Edits use a small override file
beside the imported preference XML so the source import and its unrelated
preferences remain intact.

This implementation covers only the joystick and three action buttons. It does
not yet reproduce the original collision rejection, the other draggable
sprites, the ten-float preference set, or the quick-use toolbar updates. The
source-function label describes the directly observed function role, not
complete behavioral parity.

## Corpus label

[`control-layout-editor-labels.jsonl`](control-layout-editor-labels.jsonl)
adds a gold `function_role` label for `ccTouchMoved`. Ghidra's recovered
branches directly show sprite selection state, bounded position updates,
intersection checks, and Cocos sprite-position updates.
