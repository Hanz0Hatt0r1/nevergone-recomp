# SingleSelectHero lifecycle and transition evidence

This note records clean-room control-flow evidence recovered from the checked-in Ghidra metadata index. It supplements `docs/single-select-hero.md` and deliberately avoids decompiler output or original binary contents.

## Exit / resource teardown

`SingleSelectHero::onExit` is at Ghidra address `0x00436fdc`.

Resolved callees show this sequence:

1. `cocos2d::CCLayer::onExit`
2. `CCDirector::sharedDirector`
3. `CCTouchDispatcher::removeDelegate`
4. four calls to `CCSpriteFrameCache::removeSpriteFramesFromFile`
5. `CCTextureCache::sharedTextureCache`
6. `CCTextureCache::removeUnusedTextures`

For the recompilation this is evidence that SingleSelectHero-owned visual resources should not remain permanently resident after the scene is left. The custom GLES renderer does not use Cocos sprite-frame caches, so the equivalent cleanup is release of scene-owned GLES textures when the recovered route is no longer active.

The current compositor applies that mapping to the reconstructed base background: imported pixels remain as CPU-side reload backing, but the GLES texture is created only while the offline startup route is `opening-dialogue`. Leaving that route deletes the texture; re-entering the route recreates it from the imported backing pixels. This avoids keeping a scene-owned GPU resource resident while still allowing a later reconstructed Back transition to re-enter the scene without requiring a second asset import.

## Back transition

`SingleSelectHero::menuBackToLastUI` is at `0x00437044` and resolves directly to:

- `ManagementLayer::sharedManagementLayer`
- `ManagementLayer::backToSelectHero`

The back action should therefore return through the reconstructed character-selection routing rather than inventing a login-screen transition.

## Confirm transition

`SingleSelectHero::menuConfirm` is at `0x0043745c`.

Resolved callees include:

- `SimpleAudioEngine::playEffect`
- `ActionDataManager::LoadPlayerActionDataWithFile`
- `MEPlayer::setPlayerIdx`
- `GameSaveData::createGameSaveData`
- `GameSaveData::create`
- `AppParameters::game_startTheTime`
- `EquipManager::resetRedTipSign`
- `ManagementLayer::GoToGameLayer`
- `CharacterNameLayer::create`
- `CharacterNameLayer::CretaUI`

The exact branch condition between entering the game layer and opening the character-name layer is not represented by the broad call graph and must not be guessed. `tools/ghidra/ExportFunctionOperands.java` can be used against this function if numeric/branch evidence is needed later.

A legacy GameCenter call is also present in the original function. It is platform integration rather than a requirement for the clean-room offline route.

## Touch handling

`SingleSelectHero::ccTouchEnded` is at `0x00436edc`. The resolved calls are limited to touch-location retrieval, node-space conversion, and storing a `CCPoint`. No direct scene transition is resolved from `ccTouchEnded` itself.

## Related audio evidence

The checked-in string/archive metadata confirms:

- `SingleSelectHero_bj.mp3` is referenced by `OpeningDalogue::initUI`; the original asset resides at `assets/sound/SingleSelectHero_bj.mp3`.
- `SingleLogin_UI/SingleSelectHero/audio/OpenTheDoor.mp3`
- `SingleLogin_UI/SingleSelectHero/audio/CloseTheDoor.mp3`
- `SingleLogin_UI/SingleSelectHero/audio/Heart_Siow.wav`

A focused metadata probe additionally resolves `SingleSelectHero::FuncBegin` at `0x0043705c`. It constructs action/fade sequences, installs two repeating `FadeTo` loops, and calls `playEffect` for `Heart_Siow.wav`. Exact positions, durations and the callback timing that enters `FuncBegin` still require focused numeric operand evidence and are not guessed in the runtime.
