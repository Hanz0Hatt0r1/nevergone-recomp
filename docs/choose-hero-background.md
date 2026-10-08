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

`ChooseHeroBackgroundComposer` extracts all ten staged frames through the shared TexturePacker restoration path. For the foreground clouds, the compositor uses the restored source width/height as the original Cocos `CCSprite::getContentSize()` and applies the TexturePacker trim offset inside that source canvas before anchor placement.

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
- creates six `shandian%02d.png` lightning sprites at z-order 20 with opacity 0;
- creates six `menlei%02d.png` thunder-related sprites at z-order 20 with opacity 0;
- creates centered `diguang.png` at z-order 20 with opacity 0;
- calls the separate random-thunder path after those invisible effect nodes exist.

The deterministic first visible layer is therefore a six-second fade of `bejingwuyun.png`. Lightning, thunder visuals and ground light begin invisible and remain deferred until their separate random-effect behavior is reconstructed.

For comparison, focused evidence also shows `PartTow()` fades `bejingyueliang.png` in over 8 seconds, while `CreateSun()` repeats a 3-second fade-in / 3-second fade-out on `bejingyueliang01.png`; neither is on the effective shipped initial `createUI()` sequence described above.

## Recovered `BalckCloud()` motion

`BalckCloud()` is deterministic and creates six sprites at z-order 30: two instances of each foreground-cloud frame. All six use anchor `(1.0, 0.5)` and repeat their complete action sequence forever. The zero-duration `CCMoveTo` at the end of each sequence is the wrap/reset operation.

`qianjingyun02.png` keeps full opacity and uses the top band `y = visibleHeight - contentHeight/2`:

- instance A starts at `x = 0`, moves for 40 seconds to `visibleWidth + contentWidth`, then resets to `x = 0`;
- instance B starts at the same position, waits 20 seconds, performs the same 40-second move, then resets; the 20-second delay repeats on every cycle.

`qianjingyun03.png` sets opacity to 178 and uses `y = 100 + contentHeight/2`:

- instance A starts at `x = -contentWidth`, moves for 60 seconds to `visibleWidth + contentWidth`, then resets to `-contentWidth`;
- instance B starts at `x = -2*contentWidth`, waits 30 seconds, performs the same 60-second move, then resets to `-2*contentWidth`.

`qianjingyun01.png` also sets opacity to 178, but the two copies occupy separate vertical bands:

- instance A uses `y = 350 + contentHeight/2`, starts at `x = -contentWidth`, moves for 50 seconds to `visibleWidth + contentWidth`, then resets;
- instance B uses `y = 400 + contentHeight/2`, starts at `x = -2*contentWidth`, waits 25 seconds, moves for 50 seconds to `visibleWidth + contentWidth`, then resets.

The recomp models these exact `CCDelayTime -> CCMoveTo -> CCMoveTo(0) -> CCRepeatForever` contracts on the reconstructed game clock rather than inferring motion from texture size or visual appearance.

## Current runtime boundary

When the reconstructed offline startup route becomes `choose-role`, `GameSurfaceView` lazily decodes the two confirmed background atlases from app-private imported assets, runs the ten recovered frames through `ChooseHeroBackgroundComposer`, and uploads their restored pixel/placement metadata into a synchronized native scene-owned backing store. Leaving `choose-role` clears that backing store. A hot asset re-import also clears it so a later route entry uses the refreshed user-owned files.

The native compositor consumes the exact full-canvas `bejingwuyun.png` frame and reproduces the recovered `CCFadeIn(6.0f)` using the existing 35 Hz reconstructed game clock. It then draws the six recovered `BalckCloud()` sprites above it in their original z-order relationship. Foreground-cloud textures preserve TexturePacker trim placement inside the original source canvas before applying the recovered anchor.

The remaining conservative boundary is the random lightning/thunder/ground-light behavior created by `PartThree()`. Those effect sprites are staged by the atlas importer but are not made visible until their random scheduling and callback semantics are separately recovered.

No original image/audio bytes or instruction dumps are stored in the repository; only the recovered behavioral contract is recorded here.
