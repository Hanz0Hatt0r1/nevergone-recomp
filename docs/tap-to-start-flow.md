# TapToStart reconstruction evidence

This note records the clean-room evidence used by the reconstructed TapToStart input gate.

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

## Current compatibility boundary

The runtime currently stops at the recovered `platformAutoLogin` request boundary. It does not invent credentials, network responses, or the obsolete third-party Android login SDK. A later offline-compatibility step can consume that one-shot request and reproduce the verified callback-0 local path into `OnSelectCharacter()`.
