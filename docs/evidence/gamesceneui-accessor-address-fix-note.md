# GameSceneUI GetControlsObj accessor address correction

The original accessor probe used the ELF symbol address `0x003d6eee` directly in the analyzed Ghidra project and therefore could resolve the wrong function in the current image-base view.

The current Ghidra project represents this binary with the established `+0x10000` image-base view, so the accessor probe uses `0x003e6eee`. The runner also validates that every exported row reports `function_name=GetControlsObj` before the offline audit can run.

This correction is evidence-only and does not alter receiver inference, query emission, or routing.
