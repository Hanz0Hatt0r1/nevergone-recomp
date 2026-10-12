# SceneActions current-frame resource boundary — 2026-10-12

## Recovered/runtime basis

The merged TrapActions document store already retains the born and idle `.actData` documents selected for the recovered `klhuo` SceneActions pair. `SceneActionsSystem::waUpdate(float)` now exposes a current primary-frame index, and the merged primary-frame plan proves that each selected primary ActionFrameData supplies its atlas plist at `+0x60` and sprite-frame name at `+0x68`.

## Project boundary

`scene_actions_current_frame_resource::build()` projects exactly those two resource identifiers for one explicit frame index and the two proven klhuo modes (`0` born, `1` idle). It fails closed for out-of-range frames, unsupported modes, or empty resource names.

`SceneActionPairRuntimeLoader.currentFrame(slot, frameIndex)` retrieves the already parsed retained document through JNI; it does not reread or reparse the `.actData` file. `SceneActionInitialFrameResolver` now accepts both the original initial-frame request and the new current-frame request through the same recovered Cocos atlas search/TexturePacker metadata path.

## Deliberate limits

This increment does not yet upload or replace the retained SceneActions pixel slot, advance the playback clock, or draw the frame. Geometry remains owned by `scene_actions_primary_frame_plan`; pixel storage/render switching is the next runtime boundary. No variant is inferred from file availability and no proprietary assets are committed.
