# P7 player atlas cache fix

The current physical-device P7 boundary reaches a live GameLevels render queue and stages two scene assets before failing on the first player-body sprite-frame request. The action document names `p01_02_default.plist` and frame `P01_I01_001.png`.

A bounded inventory of the user-owned original APK/OBB shows that the exact action-specific plist is absent from the base files, while the recovered `MEPlayer::createActions()` preload family `p01_02_01_default.plist` through `p01_02_08_default.plist` is present and the requested frame occurs exactly once, in `_01`. `_09` is absent from both base APK and OBB.

The reconstructed stager now models that proven cache order instead of requiring the action-specific plist to exist before any lookup:

1. resolve only the evidence-backed `pNN_CC_01..09_default.plist` candidates in numeric order when they are physically present;
2. append the exact action-specific plist only when it is physically present;
3. build the existing Cocos-compatible first-loaded-wins frame cache from that ordered list;
4. perform the requested sprite-frame lookup normally.

The change does not scan unrelated player atlases or synthesize missing resources. Non-`pNN_CC_default.plist` action names remain exact-only.
