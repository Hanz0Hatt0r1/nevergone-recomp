# BeatenEffect script-engine field-load frontier v40

Real v39 evidence reduces the packaged `CCScriptEngineManager` writer surface to one non-null writer (`setScriptEngine(this, param1)`) plus one zero clear (`removeScriptEngine(this)`). It also shows 33 `sharedManager()` call sites across 30 callers, and every bounded use of the returned singleton pointer is a `LOAD`; no direct member call or other first-level use occurs.

v40 follows exactly that shape without changing runtime routing.

## Evidence collected

`ExportAccessorFieldLoadConsumers.java` resolves `cocos2d::CCScriptEngineManager::sharedManager` by exact analyzed symbol, enumerates all direct callers, and follows the accessor return through only transparent value operations and constant pointer arithmetic. The first dereference is emitted as a manager-relative field load with an exact offset. The loaded value is then followed separately and classified as:

- receiver call (`CALL`/`CALLIND` argument 0 / P-code input 1);
- non-receiver call argument;
- retention store;
- return escape;
- dereference read/write;
- null check;
- opaque use.

The v40 audit is fail-closed against the real v39 shape. It requires all 33 `sharedManager()` call sites to have exactly one first dereference and requires every dereference to be the v39 engine field at `+0x0`. Any manager-result non-field use, shifted offset, incomplete caller decompilation, or count drift rejects the stage.

The runner also captures focused High-P-code for exact-symbol `sharedManager()` and `purgeSharedManager()`. Those files are intentionally not interpreted as backing-cell closure in v40; they are the bounded input for the next singleton backing-cell/reference stage.

## Deliberate boundary

v40 may close the `sharedManager()` → engine-field dereference frontier and may show whether the loaded engine pointer itself escapes. It still does not prove that the engine pointer is ever non-null at runtime, and it does not bound direct accesses to the singleton backing cell. External/injected runtime code remains outside the packaged-program proof.

Therefore:

- `engine_call_runtime_reachability_proven = false`;
- `script_side_effects_closed = false`;
- `whole_program_alias_writes_to_manager_engine_slot_excluded = false`;
- `receiver_promotions = 0`;
- `affects_routing = false`.

## Reproduce

With v39 already present:

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only
bash tools/ml/run_nevergone_beaten_script_engine_field_load_v40.sh
```

If v39 must be regenerated, pass the original APK:

```bash
bash tools/ml/run_nevergone_beaten_script_engine_field_load_v40.sh \
  /home/pes/ghidra_projects/nevergone/com.hippiegame.nevergone.apk
```

Primary archive:

```text
$HOME/nevergone-beaten-script-engine-field-load-v40.tar.gz
```
