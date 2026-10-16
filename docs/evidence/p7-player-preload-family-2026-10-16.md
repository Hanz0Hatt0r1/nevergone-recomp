# P7 player preload-family boundary (2026-10-16)

Physical-device P7 evidence reached the first player-body `sprite-frame-by-name` request after scene construction, render-queue creation, player spawn/motion, scene atlas parsing, and two scene requests had already succeeded. The remaining staging failure was `player-atlas-resolution-failed` for the action-frame plist name `p01_02_default.plist` and frame key `P01_I01_001.png`.

A bounded inventory check against the user-owned original `main.9.com.hippiegame.nevergone.obb` established:

- the action-specific `p01_02_default.plist` is absent;
- the recovered `MEPlayer::createActions()` preload family `p01_02_01_default.plist` through `p01_02_08_default.plist` is present;
- `p01_02_09_default.plist` is absent from the base OBB (and the base APK does not supply that family member);
- `P01_I01_001.png` occurs as bytes in `p01_02_01_default.plist`, but its actual frame key is `02P01_I01_001.png`; no member of the `p01_02` family has the exact requested key or an alias for it.
- Other selector families do contain the exact key (`p01_03_01_default.plist` and `p01_04_01_default.plist`), but the current action record names `p01_02_default.plist`. Their presence does not prove that native preloaded either family for this player state.

The reconstruction must not require the action-specific plist before consulting the already-preloaded player sprite-frame cache. The bounded runtime rule is: resolve the proven `pNN_CC_01..09_default.plist` family in numeric order when present, append the exact action-specific plist only when it is physically available, then perform an exact frame or alias lookup with normal Cocos first-loaded canonical-name precedence. The current user-owned data still fails that lookup. Do not strip the `02` prefix or search unrelated selector families without native evidence that they were loaded.

The original diagnostic script used Python `plistlib`, which rejected the shipped legacy Apple DTD terminator before checking keys. It now normalizes only that exact terminator, then compares parsed frame keys and aliases. A raw substring hit was not evidence of SpriteFrameCache membership.

The Android staging path now publishes a hidden entry for this unresolved player-body request while continuing to stage the scene's other sprites. Its runtime report counts `missing player frames` so this bounded P7 scene-render attempt cannot be mistaken for a visible P8 player. The P8/P9 device-proof validator rejects any nonzero count. A fresh contiguous device capture is still required before P7 can be marked accepted; the exact player-frame source remains unresolved.

The rebuilt debug APK installed and launched on the Redmi Note 8 Pro through REA. The immediate post-install capture stayed at the fresh-role name-entry route (`GameLevels` boundary idle), so it does not verify scene rendering or change the P7 acceptance state. Build, parser, and validator smoke checks passed.

No original asset bytes are stored in the repository; only derived names and membership observations are retained here.
