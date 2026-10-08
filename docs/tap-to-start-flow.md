# TapToStart reconstruction evidence

This note records the clean-room evidence used by the reconstructed TapToStart input gate and the offline startup route that follows it.

## Recovered control flow

Ghidra metadata and ARMv7 disassembly identify the following entry path:

- `ManagementLayer::initLoginLayer()` creates `SingleLoginLayer`, then calls `ManagementLayer::GoToTapToStart()`.
- `GoToTapToStart()` removes the prior data/UI layer, creates `TapToStart`, stores it as the current entry layer, and adds it above SingleLogin.
- `TapToStart::ccTouchBegan()` calls `TapToStart::OnTapScreen()` immediately and returns `true`.
- `OnTapScreen()` accepts input only while the internal state equals `4`. It changes the state to `5` before handing control to `AndroidSdkMgr::platformAutoLogin()`.
- `ManagementLayer::androidSdkCallBack()` routes callback code `0` to `TapToStart::OnLogin()` and callback code `-101` to the legacy Android login menu.
- During normal `ManagementLayer::init()`, the branch byte used by `OnLogin()` is initialized to zero. The default success path therefore calls `AppParameters::loadRes()` and then `ManagementLayer::OnSelectCharacter()`; the alternate non-zero branch goes to `GoToLoginScreen()`.

Relevant Ghidra addresses are `0x003063c2` (`GoToTapToStart`), `0x003063ec` (`initLoginLayer`), `0x00306ca6` (`androidSdkCallBack`), `0x0030c400` (`OnTapScreen`), `0x0030c440` (`ccTouchBegan`), `0x0030c454` (`OnLogin`) and `0x0030ca18` (`TapToStart::init`).

## Resource/update states

The original TapToStart implementation also owns legacy Android OBB extraction/update UI. Recovered references include:

- `main.9.com.hippiegame.nevergone.obb`
- `GdUI02`
- `Update_Bar1.png`
- `Update_Bar2.png`
- `Client_Extract`

Those stages use internal states `1..3` before the layer reaches interactable state `4`. The recomp already imports/decodes the user-owned original resources before the recovered splash sequence starts, so it does not reproduce the obsolete OBB extraction thread. When SingleLogin becomes active, the modern resource path enters the recovered interactable state `4` directly.

## Offline role route

The obsolete Android login SDK is not reproduced. The clean-room compatibility path consumes the one-shot recovered `platformAutoLogin` request and applies the verified success callback (`androidSdkCallBack(0)`), which reaches `OnLogin()` and then `OnSelectCharacter()`.

The local character branch is recovered separately from the network/Lua role-list callbacks:

- `ManagementLayer::OnSelectCharacter()` is at `0x00307154` and calls `GameSaveData::LoadStandaloneHeroDataList()`.
- `ManagementLayer::ReadIcloud()` is at `0x00307028`.
- With no standalone hero, `ReadIcloud()` calls `ManagementLayer::initOpeningDalogue()` at `0x003062a0`.
- With an existing standalone hero, it calls `ManagementLayer::GoToChooseRole()` at `0x00306454` and `GameSceneUI::showChooseHeroPane()` at `0x003e6a20`.
- `initOpeningDalogue()` creates the opening/create-character stack including `OpeningDalogue` and `SingleSelectHero`.

`GameSaveData::LoadStandaloneHeroDataList()` is at `0x0034ba90`. Its recovered string reference formats standalone saves as `DMG_%02d.sData` under the Cocos writable path. The recomp therefore probes its app-private files root for regular files matching `DMG_` + at least two decimal digits + `.sData`, without assuming a fixed number of slots.

The resulting runtime routes are intentionally semantic rather than a port of GameCenter/iCloud code:

- no matching standalone save -> `opening-dialogue`
- matching standalone save present -> `choose-role`

The visual `OpeningDalogue`/`SingleSelectHero` and choose-role panes are subsequent reconstruction milestones; until those layers are implemented, the route is exposed through bootstrap diagnostics while the recovered SingleLogin renderer remains intact.
