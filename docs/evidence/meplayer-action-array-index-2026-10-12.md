# MEPLAYERSTATE to action plist key — 2026-10-12

## Clean-room boundary

This note records bounded static behavior recovered from the shipped ARMv7 `libcocos2dcpp.so`. No original binary, disassembly listing, plist, action data, or proprietary asset is committed.

## Array selection

`MEPlayer::setPlayerStateWithIdx(MEPLAYERSTATE, unsigned int)` stores the requested state at MEPlayer `+0x278`, retrieves the current player-mode action array, and calls `CCArray::objectAtIndex()` with the MEPLAYERSTATE value directly. The returned `MeActionsSystem` becomes MEPlayer `+0x234` and is then played.

Therefore MEPLAYERSTATE is the zero-based array index, not an unrelated lookup code.

## Array construction

`MEPlayer::createActions(unsigned int)` builds the same array from `p%02d_actions.plist`. Its loop starts with array index 0 and formats the plist key with the shipped string `action%02d` using `index + 1` before looking up each action entry and appending its `MeActionsSystem` to the array.

The resulting mapping is:

`MEPLAYERSTATE N -> pNN_actions.plist key action(N + 1), formatted with at least two decimal digits.`

Examples relevant to the recovered Controls path:

| MEPLAYERSTATE | Plist key | Current meaning |
| ---: | --- | --- |
| `0x02` (2) | `action03` | A base action |
| `0x11` (17) | `action18` | B guarded action |
| `0x1b` (27) | `action28` | existing initial player action |
| `0x1d` (29) | `action30` | B fallback action |
| `0x20` (32) | `action33` | C / jump |

This also independently explains why the existing initial-action resolver was correctly using `action28`: the initial action represented by the current runtime is state/index `0x1b`.

## Project boundary

`PlayerActionAssetResolver.actionKeyForState()` now implements this recovered mapping for the observed MEPLAYERSTATE range `0x00..0x43`, and `resolveState()` resolves the corresponding user-imported action data through the same existing plist/actionsData path. No action is executed merely by resolving it; live state transition remains a separate runtime boundary.
