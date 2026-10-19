# GameScene camera live check: 2026-10-16

## Setup

The original package and recompilation were exercised on the same Redmi Note 8 Pro at 2340x1080 in landscape. No save data was cleared. Both apps were in the tutorial room; the original showed the `Run` lesson. In the recompilation, the developer panel was hidden before taking gameplay input.

## Observed follow behavior

On the original, two rightward joystick drags moved Dawn toward the right-side focus point. A further rightward drag kept Dawn in roughly the same screen area while the stonework moved left. This matches the original `0.5 + 0.12`-of-visible-width focus point described in [game-scene-camera-parallax-2026-10-12.md](game-scene-camera-parallax-2026-10-12.md); the live captures do not measure individual parallax factors.

On the recompilation, a 2.5-second rightward joystick drag moved the room left by about 384 screen pixels, measured by matching the background crop at `(200, 180)-(1100, 400)` between frames. The reconstructed player's blue-pixel bounding-box center changed from x=1483 to x=1453, so the room scrolled while the player stayed near the same screen position. The joystick remained screen-fixed.

This confirms that the current live horizontal follow path runs on-device. The recompilation comparison is a movement-response check, not an exact dead-zone parity test: it used an already-retained tutorial run, and the two player renderings differ. Leftward follow gates, queued camera actions and zoom remain outside this check.

## Captures

The screenshots are local device evidence and are not committed. Their SHA-256 digests are recorded here for identification:

| Capture | Local file | SHA-256 |
|---|---|---|
| Recompilation before the rightward drag | `/tmp/nevergone-dialogue-full.png` | `1f5d5b80e41e8dd68f894d403147934744d331dc9a5aa74ba3c0f1a88e41e27e` |
| Recompilation after the rightward drag | `/tmp/nevergone-after-move.png` | `363354548727b612f0fd01545311ac870dcffff3c4750ebc6c4a19d687a07a8e` |
| Original at the Run lesson before moving | `/tmp/nevergone-original-dialogue-step.png` | `d8467d5636423378c9a7405d9b1144315418c188afed97937b007909127910fe` |
| Original after two rightward drags | `/tmp/nevergone-original-after-run.png` | `c51d44f8ec5942463a9ce555c966d823429535c3d5a07d4e39dc04a983285a03` |
| Original after continuing rightward | `/tmp/nevergone-original-run-further.png` | `a673de4bfcdcbf40d55d1655116dbe3011931a0da32336a566fdb899c65374c3` |
