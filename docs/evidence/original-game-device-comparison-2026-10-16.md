# Original game vs recompilation on device (2026-10-16)

## Capture setup

Both packages were run on the same Redmi Note 8 Pro (`begonia`, Android 13 / API 33), landscape display 2340x1080. The original `com.hippiegame.nevergone` package (version 1.0.9) and the recompilation `org.nevergone.recomp` (version 0.0.1-dev) were already installed side by side, so the original install and its saved data were left intact.

- Original app launcher: `com.hippiegame.nevergone/.TJ_P_01`; PID 21460; cold launch completed successfully.
- Recompilation was returned to the foreground after the completed P7-P10 run; the active scene showed the retained `cp_lesson` route and restored player state.
- Original gameplay screenshot SHA-256: `cc1cecf164a003b9ee2e75d7de55a669d213255ab9c56aacf298baf83f92baa3` (local capture `/tmp/nevergone-original-play.png`).
- Recompilation screenshot SHA-256: `5707068d5936eb7df6e59c50792b663498aaf621595325cbaf27b5e2967769f2` (local capture `/tmp/nevergone-recomp-clean.png`).

## Direct observations

The original opened its existing Dawn Lv.1 role. Selecting Play entered a rendered game location with the character standing near the center-left of the lit floor. The original log captured `MeActionsSystem` initialization through its reported end marker, and the process stayed alive.

The recompilation showed its fresh-role `cp_lesson` scene, reconstructed player body and input overlay. The visible player was high and left of the brightest floor region. This is a concrete position/camera comparison point for future work, but it is not a same-scene comparison: the original capture is a saved-role story location, while the recompilation capture is the tutorial route. The roles/costume state also differ, so the screenshots do not establish an outfit-color defect.

The two captures confirm that the imported source package runs on the target device and gives a visual baseline for native action/render behavior. They do not establish matching scene selection, spawn coordinates, camera transform, or combat behavior. A strict visual comparison needs the original game in the same tutorial scene with the same role/costume state.

## Fresh-role tutorial comparison

After the user cleared the original save, the original package cold-launched into its splash, continued through the new-story flow, and reached the tutorial's `Run` instruction. The screenshot at `/tmp/nevergone-original-fresh-8.png` has SHA-256 `8a12cd75e49c46269c765a285377e2c05dcce66b28892340a9b8f7ade284edeb`. It shows the Dawn HUD, the full direction/action control set and the instruction “Double-tap a direction to run”; the character stands on the lit floor near the lower middle-left of the room.

The recompilation was then returned to the same imported tutorial room. The fresh P7 capture at `/tmp/nevergone-original-compare-run/p7-render/scene.png` has SHA-256 `d707ae4db5dcc87dfc63f25f33d26c8702bd4d4a92bb49fa7d977aff771ba136`. It shows the reconstructed player and joystick, with the player visibly above and left of the lit floor. In this capture the top HUD and Run instruction are absent. This is now a same-room visual comparison; exact step timing and selected costume are not proven equal, so color differences are not treated as a defect yet.

The fresh P7 collector did not pass its completed-live-revision gate on this rerun. Diagnostics reported 9 render requests and 9 staged requests, but only 7 scene render sprites and zero GameScene staging attempts/successes. The retained diagnostic report SHA-256 is `f28f29f720897214ea36362d8d034d74899d5ccd834397621dc1e02508d2b7a4`. The previous contiguous P7-P10 run had passed with all 9 staged/drawn sprites, so this comparison run exposes a reproducibility issue as well as a likely spawn/camera alignment gap.

## Repeat after completing the recompilation onboarding

The first P7 attempt from the fresh launch stopped on the opening narrative: GameLevels was still idle, there was no live render queue, and the original-story screen remained visible. Replaying the established onboarding taps reached the live scene; the render collector then passed with 9 drawn sprites, 9 staged requests and 0 hidden slots. Full P7-P10 passed again on the same device at `/tmp/nevergone-live-20261016/final-p7-p10/proof.md`; process `20869` restarted as `24570`, and the non-zero motion offset `(-125.7237, 187.6302)` was restored.

The same-room comparison still shows a measurable vertical placement mismatch. In the original Run frame the character's feet are near the lower lit floor, while the recompilation's player is well above that floor. The horizontal placement is close. I am keeping this as an observed gap rather than changing the recovered `source_y + 40 - win_height/2` formula without stronger evidence about the original camera/player anchor. The original Run prompt and top HUD also remain absent from the recompilation capture.

Next visual acceptance work should first make the fresh tutorial capture stable, then align the rendered player to the original lit-floor spawn and restore the matching tutorial HUD/instruction state. The prior P7-P10 result remains valid for that earlier passing run; this failed repeat is retained as a new limitation, not substituted for it.
