# ChooseHero background reconstruction evidence

This note records clean-room evidence for the existing-save `choose-role` route. It does not contain original asset bytes or decompiler output.

## Entry path

`GameSceneUI::showChooseHeroPane()` creates `ChooseHero`, hides the normal player/control UI, and adds the pane. `ChooseHeroBackground::init()` then loads its recovered background atlases, stops the prior background music, creates foreground-cloud behavior, starts the background callback sequence, and creates `ChooseHeroReadyUIScene`.

Recovered function addresses include:

- `ChooseHeroBackground::BalckCloud` — `0x002ee054`
- `ChooseHeroBackground::PartTow` — `0x002ee720`
- `ChooseHeroBackground::CreateSun` — `0x002ee7f4`
- `ChooseHeroBackground::PartThree` — `0x002eea00`
- `ChooseHeroBackground::createUI` — `0x002eecfc`
- `ChooseHeroBackground::PartOne` — `0x002ef230`
- `ChooseHeroBackground::FuncPart` — `0x002ef460`
- `ChooseHeroBackground::LoadingTexture` — `0x002ef4e4`
- `ChooseHeroBackground::init` — `0x002ef51c`

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

The background phases reference these confirmed frames:

- `PartOne`: `yueliang.png`, `yueliangzhezhao.png`, `xingkong.png`, plus generated stars and meteors.
- `PartTow`: `bejingyueliang.png`.
- `CreateSun`: `bejingyueliang01.png`.
- `PartThree`: `bejingwuyun.png`, `shandian%02d.png`, `menlei%02d.png`, `diguang.png`.
- `BalckCloud`: `qianjingyun01.png`, `qianjingyun02.png`, `qianjingyun03.png`.

Checked-in TexturePacker metadata confirms `bejingyueliang.png` and `bejingwuyun.png` are exact, non-rotated 1136x640 full-canvas frames. `yueliang.png`, `yueliangzhezhao.png`, `xingkong.png`, `bejingyueliang01.png`, and `diguang.png` are trimmed frames whose source canvas is also 1136x640, so their atlas offsets are sufficient to reconstruct exact design-canvas placement.

`ChooseHeroBackgroundComposer` extracts these frames through the shared TexturePacker restoration path and verifies those source-canvas invariants. The three foreground-cloud frames are also extracted, but their scene positions remain action/operand-driven and are intentionally not inferred from their independent texture sizes.

## Current runtime staging boundary

When the reconstructed offline startup route becomes `choose-role`, `GameSurfaceView` now lazily decodes the two confirmed background atlases from app-private imported assets, runs the ten recovered frames through `ChooseHeroBackgroundComposer`, and uploads their restored pixel/placement metadata into a native scene-owned backing store. Leaving `choose-role` clears that backing store. A hot asset re-import also clears it so a later route entry uses the refreshed user-owned files.

The native store preserves the 1136x640 design-canvas contract for the seven phase layers while allowing the three foreground-cloud frames to retain their independent TexturePacker source sizes. It does not create or draw a visual phase yet.

## Unresolved phase dispatch

`ChooseHeroBackground::FuncPart` contains references to `PartOne`, `PartTow`, `CreateSun`, and `PartThree`. The broad Ghidra metadata index confirms those targets but does not contain the immediate scalar/branch operands needed to determine the exact phase condition and timing. Therefore the current runtime does **not** choose or draw a background phase yet.

The focused `tools/ghidra/ExportFunctionOperands.java` exporter should be run against `FuncPart` and `createUI` in the analyzed local Ghidra project before phase rendering is connected. This keeps the implementation evidence-driven instead of selecting a plausible-looking background by guesswork.
