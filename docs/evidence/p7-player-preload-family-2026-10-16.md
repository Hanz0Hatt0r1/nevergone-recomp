# P7 player preload-family boundary (2026-10-16)

Physical-device P7 evidence reached the first player-body `sprite-frame-by-name` request after scene construction, render-queue creation, player spawn/motion, scene atlas parsing, and two scene requests had already succeeded. The remaining staging failure was `player-atlas-resolution-failed` for the action-frame plist name `p01_02_default.plist` and frame key `P01_I01_001.png`.

A bounded inventory check against the user-owned original `main.9.com.hippiegame.nevergone.obb` established:

- the action-specific `p01_02_default.plist` is absent;
- the recovered `MEPlayer::createActions()` preload family `p01_02_01_default.plist` through `p01_02_08_default.plist` is present;
- `p01_02_09_default.plist` is absent from the base OBB (and the base APK does not supply that family member);
- `P01_I01_001.png` occurs exactly once in the available family, in `p01_02_01_default.plist`.

The reconstruction therefore must not require the action-specific plist before consulting the already-preloaded player sprite-frame cache. The bounded runtime rule is: resolve the proven `pNN_CC_01..09_default.plist` family in numeric order when present, append the exact action-specific plist only when it is physically available, then perform the frame lookup with normal Cocos first-loaded canonical-name precedence. Missing optional family members are not replaced by unrelated atlas scans.

No original asset bytes are stored in the repository; only derived names and membership observations are retained here.
