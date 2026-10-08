# ChooseHero background reconstruction evidence

This note records clean-room evidence for the existing-save `choose-role` route. It does not contain original asset bytes or decompiler output.

## Entry path

`GameSceneUI::showChooseHeroPane()` creates `ChooseHero`, hides the normal player/control UI, and adds the pane. `ChooseHeroBackground::init()` then loads its recovered background atlases, stops the prior background music, creates foreground-cloud behavior, starts the background callback sequence, and creates `ChooseHeroReadyUIScene`.

Recovered Ghidra function addresses include:

- `ChooseHeroBackground::BalckCloud` — `0x002ee054`
- `ChooseHeroBackground::PartTow` — `0x002ee720`
- `ChooseHeroBackground::CreateSun` — `0x002ee7f4`
- `ChooseHeroBackground::PartThree` — `0x002eea00`
- `ChooseHeroBackground::createUI` — `0x002eecfc`
- `ChooseHeroBackground::PartOne` — `0x002ef230`
- `ChooseHeroBackground::FuncPart` — `0x002ef460`
- `ChooseHeroBackground::LoadingTexture` — `0x002ef4e4`
- `ChooseHeroBackground::init` — `0x002ef51c`

A focused Thumb/ARM inspection of the user-owned shipped library confirms that `init()` calls `LoadingTexture()`, stops the previous background music, calls `BalckCloud()`, calls `createUI()`, and then creates/adds `ChooseHeroReadyUIScene`.

## Recovered atlases

`LoadingTexture()` loads:

- `LevelUI/Gate_Background_UI/Gate_BackgroundPNG_01.plist`
- `LevelUI/Gate_Background_UI/Gate_BackgroundPNG_02.plist`
- `LevelUI/Gate_Background_UI/eagle/eaglePNG.plist`

A metadata-only archive probe also confirms the exact user-supplied OBB paths for the two background atlas pairs consumed by the current runtime staging layer:

- `assets/gamescene_ui/LevelUI/Gate_Background_UI/Gate_BackgroundPNG_01.plist`
- `assets/gamescene_ui/LevelUI/Gate_Background_UI/Gate_BackgroundPNG_01.png`
- `assets/gamescene_ui/LevelUI/Gate_Background_UI/Gate_BackgroundPNG_02.plist`
- `assets/gamescene_ui/LevelUI/Gate_Background_UI/Gate_BackgroundPNG_02.png`

The corresponding image bytes are still supplied only by the user's imported original resources at runtime.

## Recovered layer frames

The background functions reference these confirmed frames:

- `PartOne`: `yueliang.png`, `yueliangzhezhao.png`, `xingkong.png`, plus generated stars and meteors.
- `PartTow`: `bejingyueliang.png`.
- `CreateSun`: `bejingyueliang01.png`.
- `PartThree`: `bejingwuyun.png`, `shandian%02d.png`, `menlei%02d.png`, `diguang.png`.
- `BalckCloud`: `qianjingyun01.png`, `qianjingyun02.png`, `qianjingyun03.png`.

Checked-in TexturePacker metadata confirms `bejingyueliang.png` and `bejingwuyun.png` are exact, non-rotated 1136x640 full-canvas frames. `yueliang.png`, `yueliangzhezhao.png`, `xingkong.png`, `bejingyueliang01.png`, and `diguang.png` are trimmed frames whose source canvas is also 1136x640, so their atlas offsets are sufficient to reconstruct exact design-canvas placement.

`ChooseHeroBackgroundComposer` extracts these frames through the shared TexturePacker restoration path and verifies those source-canvas invariants. For the three foreground clouds, the runtime now combines their restored TexturePacker trim placement with the recovered Cocos sprite anchor/position instead of inferring scene placement from texture size.

## Effective shipped `createUI()` dispatch

Focused instruction-level inspection resolves an important ambiguity left by the broad metadata index. `createUI()` constructs six `CCCallFuncND` callback objects carrying integer payloads `0` through `5`, and `FuncPart` maps those payloads as follows:

```text
0 -> PartOne
1 -> PartTow
2 -> CreateSun
3 -> PartThree
4 -> cleanup of the PartThree/ground-light nodes
5 -> cleanup of the earlier background node
```

However, the shipped `createUI()` retains only the callback object carrying payload `3` when it constructs the final `CCSequence`. The callback objects for `0`, `1`, `2`, `4`, and `5` are created/autoreleased but are not passed into that sequence. The effective initial path is therefore **`PartThree()` only**, not a guessed `PartOne -> PartTow -> CreateSun -> PartThree` progression.

This distinction matters for the recompilation: the dormant helper functions remain useful evidence for possible alternate/legacy transitions, but they must not be presented as the normal initial ChooseHero animation unless another verified caller reaches them.

## Recovered `PartThree()` visible subset

`PartThree()` performs the following verified setup:

- creates a scene node and adds it at z-order 10;
- creates centered `bejingwuyun.png` and adds it at z-order 10;
- sets that sprite's opacity to 0;
- runs `CCFadeIn(6.0f)` on it;
- creates six `shandian%02d.png` lightning sprites at z-order 20 with opacity 0 and stores them in the member array at offset `+0x190`;
- creates six `menlei%02d.png` thunder-related sprites at z-order 20 with opacity 0 and stores them in the member array at offset `+0x194`;
- creates centered `diguang.png` at z-order 20 with opacity 0 and stores it at `+0x18c`;
- calls `RamodThunder()` after those invisible effect nodes exist.

The deterministic first visible layer is therefore a six-second fade of `bejingwuyun.png`. For comparison, focused evidence also shows `PartTow()` fades `bejingyueliang.png` in over 8 seconds, while `CreateSun()` repeats a 3-second fade-in / 3-second fade-out on `bejingyueliang01.png`; neither is on the effective shipped initial `createUI()` sequence described above.

## Recovered `BalckCloud()` motion

`BalckCloud()` is part of the shipped `init()` path and creates six foreground-cloud sprites at z-order 30. All six use anchor `(1.0, 0.5)`, so their Cocos X coordinate represents the right edge of the untrimmed sprite-frame source rectangle. The two dim cloud groups use opacity 178; `qianjingyun02` stays at the default full opacity.

The exact repeated motion is:

| Frame | Copy | Start X | Y | Delay | Move | End X | Opacity |
| --- | ---: | ---: | --- | ---: | ---: | --- | ---: |
| `qianjingyun02.png` | 0 | `0` | `H - h/2` | `0s` | `40s` | `W + w` | 255 |
| `qianjingyun02.png` | 1 | `0` | `H - h/2` | `20s` | `40s` | `W + w` | 255 |
| `qianjingyun03.png` | 0 | `-w` | `100 + h/2` | `0s` | `60s` | `W + w` | 178 |
| `qianjingyun03.png` | 1 | `-2w` | `100 + h/2` | `30s` | `60s` | `W + w` | 178 |
| `qianjingyun01.png` | 0 | `-w` | `350 + h/2` | `0s` | `50s` | `W + w` | 178 |
| `qianjingyun01.png` | 1 | `-2w` | `400 + h/2` | `25s` | `50s` | `W + w` | 178 |

Here `W/H` are the 1136x640 visible design dimensions and `w/h` are each sprite frame's **untrimmed source dimensions**. Before constructing the `CCPoint` values, the shipped ARMv7 code converts computed coordinates float -> signed int -> float, which truncates half-pixel positions toward zero. The reconstructed timeline preserves that pixel-grid quantization before Cocos-style interpolation begins.

After each move, a zero-duration `CCMoveTo` resets the sprite to its starting X. The entire sequence is wrapped in `CCRepeatForever`. The delay on the second copy of each group is inside the repeated sequence rather than being a one-time startup offset. Consequently their full repeat periods are 60 seconds (`20+40`), 90 seconds (`30+60`), and 75 seconds (`25+50`). This behavior is covered by `choose_hero_black_cloud_timeline_smoke.cpp`.

For rendering trimmed TexturePacker frames, the runtime reconstructs the source rectangle from the recovered `(1.0,0.5)` anchor and then applies the atlas `left/top/width/height` placement inside that source rectangle. This keeps the original motion path based on untrimmed dimensions while drawing only the stored upright pixels.

## Recovered random thunder/lightning contract

The shipped random source is `lrand48()`, not `rand()`. Its non-negative 31-bit result is converted to float and multiplied by exact `2^-31` before the timing formulas below. `choose_hero_thunder_schedule` keeps these formulas independent from the eventual runtime RNG seed/source so the recovered behavior remains host-testable.

`RamodLightning()` and the tiny `FuncLightningBen`, `FuncLightningEnd`, and `FuncLightning` methods are no-ops in this shipped build. The visible lightning path is instead driven from `RamodThunder()` and `FuncThunderEnd()`.

For every `menlei` sprite, the initial `RamodThunder()` schedule is:

1. preserve six otherwise-unused `lrand48()` calls interleaved with array-count calls; these advance the original global RNG state;
2. choose `delay = 3 + 5*u`, so the flash waits approximately 3–8 seconds;
3. choose `fade = 0.7*u`, so the fade-out lasts approximately 0–0.7 seconds;
4. set opacity to 0;
5. run `Delay(delay) -> FadeTo(0,255) -> FuncThunderBen -> FadeTo(fade,0) -> FuncThunderEnd`;
6. if child tag `101` (`ChooseHeroReadyUIScene`) exists, call `RandomShowwshandianEff(delay, fade)` with the same timing pair.

`FuncThunderBen()` consumes one `lrand48()` value and computes `slot = trunc(abs(u - 0.01) * 30)`. Only slots `0..6` play audio; all other values are silent. The seven recovered sound paths, in slot order, are:

1. `SingleLogin_UI/L_thunder08-r.mp3`
2. `SingleLogin_UI/L_Thunder09.mp3`
3. `SingleLogin_UI/L_Thunder10.mp3`
4. `SingleLogin_UI/M_thunder_norm_2.mp3`
5. `SingleLogin_UI/M_thunder_norm_5.mp3`
6. `SingleLogin_UI/M_Thunder04.mp3`
7. `SingleLogin_UI/S_thunder_norm_1.mp3`

`FuncThunderEnd()` stops the completed `menlei` sprite and schedules its next cycle with the same `3+5*u` delay and `0.7*u` fade formulas. It also chooses six `shandian` array references using only three RNG values: the first two indices are independent, while the third selected index is repeated four times. Each index is `trunc(abs(0.01-u) * count)`.

For a selected `shandian` sprite that currently has no running action, the callback reuses the new `menlei` delay and chooses `fade = 0.5 + 0.3*u`, then runs `Delay -> FadeTo(0,255) -> FadeTo(fade,0)`. The same `(delay, fade)` pair is forwarded to `RandomShowwshandianEff` for the ready UI effect path.

The ground-light sprite is also gated on having no running action. It reuses the same delay and chooses `fade = 0.5 + 0.3*abs(0.01-u)`, then performs the same hidden -> instant full opacity -> fade-out shape. One otherwise-unused `lrand48()` call is made immediately before the ground-light running-action check and is part of the recovered global RNG advancement.

The scheduling formulas and thunder-audio slot mapping are covered by `choose_hero_thunder_schedule_smoke.cpp`. The visual renderer is intentionally deferred until the twelve `shandian/menlei` frame assets are staged cleanly from the user-imported atlas; the schedule model does not fabricate missing textures or a guessed RNG seed.

## Current runtime boundary

When the reconstructed offline startup route becomes `choose-role`, `GameSurfaceView` lazily decodes the two confirmed background atlases from app-private imported assets, runs the ten currently staged frames through `ChooseHeroBackgroundComposer`, and uploads their restored pixel/placement metadata into a synchronized native scene-owned backing store. Leaving `choose-role` clears that backing store. A hot asset re-import also clears it so a later route entry uses the refreshed user-owned files.

The first compositor consumes the exact full-canvas `bejingwuyun.png` frame and reproduces the recovered `CCFadeIn(6.0f)` using the existing 35 Hz reconstructed game clock. It does not clear the prior frame before drawing, so alpha 0 begins transparently over the previous reconstructed scene just as a Cocos sprite fade would.

The `BalckCloud` compositor then draws the six recovered cloud sprites above that background, preserving the original z-order relationship (`PartThree` background z=10, clouds z=30). It uses the same scene-generation-local 35 Hz clock, the exact repeated delay/move cycles listed above, and the user-imported atlas pixels. Both compositors recreate their GLES textures when staged asset generation changes or a surface/context recreation invalidates old texture names.

The random thunder/lightning formulas are now reconstructed as a project-owned semantic layer, but the twelve effect frames are not yet staged/drawn. The next visual increment is therefore to import `shandian01..06` and `menlei01..06`, bind them plus the already staged `diguang.png` to the recovered scheduler, and preserve the original opacity/z-order/action-running gates. Role-selection UI and scene entry remain separate work.

No original image/audio bytes or instruction dumps are stored in the repository; only the recovered behavioral contract is recorded here.
