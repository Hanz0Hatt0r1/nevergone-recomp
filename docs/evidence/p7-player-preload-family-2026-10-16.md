# Historical P7 player preload-family boundary (2026-10-16)

This records the earlier PR #698 investigation and captures, including its temporary hidden-player behavior. The current implementation supersedes the inferred preload family and bare-frame lookup: see [player clothes and sprite-frame lookup](player-clothes-frame-lookup-2026-10-16.md). It uses explicit clothes and the native prefixed-name fallback, and now renders the player. The hidden-player workaround is not part of the current branch.

Physical-device P7 evidence reached the first player-body `sprite-frame-by-name` request after scene construction, render-queue creation, player spawn/motion, scene atlas parsing, and two scene requests had already succeeded. The remaining staging failure was `player-atlas-resolution-failed` for the action-frame plist name `p01_02_default.plist` and frame key `P01_I01_001.png`.

A bounded inventory check against the user-owned original `main.9.com.hippiegame.nevergone.obb` established:

- the action-specific `p01_02_default.plist` is absent;
- the recovered `MEPlayer::createActions()` preload family `p01_02_01_default.plist` through `p01_02_08_default.plist` is present;
- `p01_02_09_default.plist` is absent from the base OBB (and the base APK does not supply that family member);
- `P01_I01_001.png` occurs as bytes in `p01_02_01_default.plist`, but its actual frame key is `02P01_I01_001.png`; no member of the `p01_02` family has the exact requested key or an alias for it.
- Other selector families do contain the exact key (`p01_03_01_default.plist` and `p01_04_01_default.plist`), but the current action record names `p01_02_default.plist`. Their presence does not prove that native preloaded either family for this player state.

The reconstruction must not require the action-specific plist before consulting the already-preloaded player sprite-frame cache. The bounded runtime rule is: resolve the proven `pNN_CC_01..09_default.plist` family in numeric order when present, append the exact action-specific plist only when it is physically available, then perform an exact frame or alias lookup with normal Cocos first-loaded canonical-name precedence. The current user-owned data still fails that lookup. Do not strip the `02` prefix or search unrelated selector families without native evidence that they were loaded.

The original diagnostic script used Python `plistlib`, which rejected the shipped legacy Apple DTD terminator before checking keys. It now normalizes only that exact terminator, then compares parsed frame keys and aliases. A raw substring hit was not evidence of SpriteFrameCache membership.

The Android staging path now publishes a hidden entry for this unresolved player-body request while continuing to stage the scene's other sprites. Its runtime report counts `missing player frames` so this bounded P7 scene-render attempt cannot be mistaken for a visible P8 player. The P8/P9 device-proof validator rejects any nonzero count. The exact player-frame source remains unresolved.

The rebuilt debug APK installed and launched on the Redmi Note 8 Pro through REA. The immediate post-install capture stayed at the fresh-role name-entry route (`GameLevels` boundary idle), so it does not verify scene rendering or change the P7 acceptance state. Build, parser, and validator smoke checks passed.

A later contiguous fresh-role capture on the same device did reach and render `cp_lesson.glData`. The retained local report `/tmp/nevergone-post-csv-complete/runtime-report.txt` has `status=pass`, timestamp `2026-10-16T01:43:00Z`, diagnostics SHA-256 `bf9c9effcce65bc963335dc9f0c60accd6a7dc7a8a0604e21b928bfb09a0cfbf` and screenshot SHA-256 `2349f35e621410525d6770c08bf526f6244f758f917ed61f3c34163034b0107f`. Diagnostics show all 9 requests staged, 8 sprites drawn, one missing player frame, visible imported lesson art, and the first dialogue completed (`line: 3/3`, interactive session, modal input unblocked). This accepts P7 for the fresh-role route while leaving P8 open.

The P7 validator previously rejected this real report because the tutorial bootstrap repeats the `render sprites` count. It now reads the count attached to `scene render queue: ready`; a regression fixture includes the second occurrence.

No original asset bytes are stored in the repository; only derived names and membership observations are retained here.
