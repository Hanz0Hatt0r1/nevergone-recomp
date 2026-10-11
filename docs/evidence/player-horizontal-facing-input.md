# Offline player horizontal facing input

The attached [High P-code rows](player-horizontal-facing-pcode.tsv) were exported from the already analyzed user-owned Nevergone 1.0.9 ARMv7 `libcocos2dcpp.so` (SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`) with `ExportFunctionPcodeFlow.java`. Ghidra addresses are ELF offsets plus `0x10000` in this project. Additional joystick facts below come from bounded ARMv7 disassembly of the same immutable library.

## Player facing effect

At Ghidra `0x0032de36`, `MEPlayer::setControlsState` compares its input with `0x12`, and at `0x0032de44` calls `MEPlayer::filpPlayer` with `true`. At `0x0032de50`, it compares with `0x13`, and at `0x0032de60` calls the same function with `false`. `MeActionsSystem::filpAction` stores the boolean at `+0x1be` (`0x0033910c`) and passes it to `CCSprite::setFlipX` on its primary sprite at `+0x118` (`0x00339114`). These are derived control-flow facts, not original binary or source assets.

The project-owned mapping therefore remains narrow: primary Controls state `0x12` produces `flipX=true`, while `0x13` produces `flipX=false`. No movement speed or position delta is inferred from those states.

## Recovered CCJoyStick input surface

The temporary lower-left drag surface introduced while the original joystick geometry was still unknown has been removed. `Controls::init()` at ELF `0x00297e2c` provides the missing shipped constants directly:

- the fixed joystick node is positioned at design coordinate `(204, 173)`;
- `CCJoyStick::create(...)` receives inner radius `10` and outer/update radius `300`;
- the touch hit radius is then set to `165`;
- the fixed-position flag is false, so a touch does not relocate the joystick center.

`CCJoyStick::containsTouchLocation(CCTouch*)` at ELF `0x002960c8` accepts a radius-mode begin only when the converted local touch distance is strictly less than `165`. `ccTouchBegan` (`0x00296300`), `ccTouchMoved` (`0x00296078`) and `ccTouchEnded` (`0x002963ec`) convert the Cocos touch through `CCDirector::convertToGL` and `CCNode::convertToNodeSpace`, so with the recovered fixed node the sample is relative to `(204, 173)` in the 1136x640 design space.

`CCJoyStick::updateTouchPoint(CCPoint)` at ELF `0x00295f00` accepts updates only while the local-vector length is at most `300 - 10 = 290`. Accepted samples retain:

- power = `length / 290`;
- angle = the local-vector `atan2(y, x)` converted to degrees;
- the corresponding normalized direction vector.

The joystick timer forwards that sample to `Controls::onCCJoyStickUpdate(...)` at ELF `0x002973dc`. The existing recovered Controls contract then applies the original `0.05` activity threshold and horizontal hemisphere mapping before the primary state reaches `MEPlayer::setControlsState`.

## Reconstructed Android route

`game_scene_player_joystick_input` now models that fixed joystick instead of an invented screen partition. The live Android `GameSurfaceView.nativeOnTouch` route is accepted only while the recovered GameScene render-queue boundary is active. Android top-left surface coordinates are converted to the shipped 1136x640 design space using the GameScene's recovered `ResolutionPolicy::ExactFit`, with independent X/Y scaling and bottom-left Cocos Y orientation.

The adapter preserves single-pointer ownership, strict begin radius `165`, update radius `290`, the native power/angle calculation, and whole-gesture `ACTION_CANCEL` behavior. Its per-frame `tick()` feeds the latest captured power/angle sample into `player_control_runtime_state`; the existing render-queue bridge changes only the body sprite flip flag. Releasing the joystick neutralizes its sample but intentionally retains the already-applied facing. Player/scene teardown clears both capture and reconstructed control state.

This provides an evidence-backed Android touch -> shipped joystick sample -> Controls state -> `MEPlayer` facing -> body `flipX` path. An on-device visual capture with user-owned imported player action/atlas data remains outstanding. Position movement, action transitions, joystick artwork rendering, and enemy interaction remain separate reconstruction boundaries and are not inferred here.
