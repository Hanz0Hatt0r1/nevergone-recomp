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
