# GameSceneUI GetControlsObj accessor address correction

The original accessor probe used the ELF symbol address `0x003d6eee` directly in the analyzed Ghidra project and therefore decompiled the containing `missionLevelUpFrame` function instead of `GameSceneUI::GetControlsObj()`.

This binary is represented in the current Ghidra project with the established `+0x10000` image-base view, so the accessor probe must use `0x003e6eee`. The runner now also validates that every exported row reports `function_name=GetControlsObj` before allowing the offline audit to run.

This correction is evidence-only and does not alter receiver inference, query emission, or routing.
