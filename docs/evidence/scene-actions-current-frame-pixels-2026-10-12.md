# SceneActions current-frame pixel staging — 2026-10-12

The retained born/idle TrapActions documents now expose an explicit current-frame resource request (`slot + frameIndex`) and the native pixel store records frame identity.

`SceneActionCurrentFramePixelStager.stage()` reuses the existing recovered Cocos atlas and TexturePacker reconstruction pipeline for that later frame. The reconstructed untrimmed ARGB pixels are uploaded through JNI with the explicit primary frame index. Native upload remains gated by the already selected variant, the born/idle slot mode and the retained document's primary-frame bounds.

`ready(slot, variant, frameIndex)` distinguishes the requested animation frame from stale pixels retained for the same slot/variant.

This increment deliberately stops before GL draw switching or automatically coupling the Java stager to `waUpdate(float)`. It provides the transactional pixel replacement boundary needed for that next integration.
