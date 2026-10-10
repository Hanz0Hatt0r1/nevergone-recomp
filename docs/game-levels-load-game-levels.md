# GameLevels LoadGameLevels top-level boundary

This note records the clean-room control-flow boundary recovered from the original Never Gone 1.0.9 ARMv7 `GameLevels::LoadGameLevels(char const*)` implementation.

## Original call chain

The shipped ELF symbol is at Thumb address `0x002c2f1d` (disassembly base `0x002c2f1c`) with a 112-byte symbol size. After constructing `HPData`, the original initializes one shared stream offset to zero and invokes the four section loaders in this exact order:

1. `LoadGL_Scene()`;
2. `LoadGL_Actions()`;
3. `LoadGL_Global()`;
4. `LoadGL_PortNode()`.

The same offset reference is forwarded to every call. A false result from Scene, Actions, or Global immediately ends loading with failure. If those three succeed, `LoadGameLevels()` returns the result of `LoadGL_PortNode()`.

No file-size/EOF comparison is performed at this level, so the clean-room runtime must treat the final PortNode end offset as the verified loader boundary rather than requiring it to equal the physical file size.

## Runtime probe boundary

`game_levels_asset_probe` now chains the recovered section parsers with the exact end offset from each preceding section. It reports distinct Scene, Actions, Global, and PortNode boundaries and exposes the final offset reached before the original `LoadGL_PortNode()` transfers into `LinkScenePortNode()`.

This verifies serialized framing through the full top-level `LoadGameLevels()` sequence without yet claiming that scene linking, object construction, or gameplay semantics have been reconstructed.

## Next step

Reconstruct the project-owned PortNode graph link pass from the now-bounded `LinkScenePortNode()` behavior, then use those links together with parsed scene records to select and construct the first offline scene.
