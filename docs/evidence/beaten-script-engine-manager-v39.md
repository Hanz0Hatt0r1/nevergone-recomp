# BeatenEffect script-engine manager frontier v39

v38 separates packaged dynamic lookup from external/runtime injection and proves that the shipped APK has no packaged `dlsym`/cross-library path to `CCScriptEngineManager::setScriptEngine`. The remaining alias risk is therefore not a packaged symbol lookup problem; it is the possibility that the singleton manager's engine field can still be written through an alias or through code outside the packaged program.

v39 narrows that residual without changing runtime routing.

## Evidence collected

The stage reuses three existing Ghidra exporters against the exact analyzed `libcocos2dcpp.so`:

1. `ExportClassParameterFieldWrites.java` over exact class `cocos2d::CCScriptEngineManager` to enumerate parameter-0-relative field stores and classify stored-value provenance.
2. `ExportClassOwnerAliasFlow.java` over the same class to require a proven owner parameter for `setScriptEngine` and retain the class-local alias frontier.
3. `ExportAccessorReturnConsumers.java` over the exact qualified symbol `cocos2d::CCScriptEngineManager::sharedManager` to enumerate every direct caller and bounded use-def consumer of the returned singleton pointer.

The real-device/Ghidra v39 run exposed why the third probe must be symbol-qualified: the packaged ELF evidence uses virtual address `0x00536cdc`, while the local analyzed Ghidra project currently places the same `sharedManager` function at `0x00546cdc`. Passing the packaged ELF address to the project caused the generic accessor exporter to resolve the containing unrelated `CCTransitionSlideInL::create` function. The runner therefore deliberately avoids a hard-coded function address for this probe. A regression smoke rejects reintroducing the old `00536cdc` invocation.

The audit derives the engine-field offset from the unique `setScriptEngine(this, param1)` field write rather than hard-coding a layout. Constant-zero writes to that same field are classified separately from non-null replacement writes. Any other manager-class write to the engine field keeps the manager writer frontier open.

The `sharedManager()` consumer export is required to have complete direct-caller decompilation. Direct member calls and unsupported/nontransparent uses are counted separately so the next stage can follow exactly the remaining accessor-result escapes instead of treating the manager as an unconstrained alias source.

## Deliberate boundary

v39 is an inventory/refinement stage, not a receiver finalizer.

Even if the manager class itself has only the setter as a non-null field writer and every direct `sharedManager()` consumer is locally understood, v39 does **not** yet claim whole-program alias closure because the singleton backing cell has not been independently enumerated for direct bypass accesses. External/injected runtime code also remains outside the packaged-program proof.

Accordingly:

- `receiver_promotions = 0`;
- `script_side_effects_closed = false`;
- `whole_program_alias_writes_to_manager_engine_slot_excluded = false`;
- `affects_routing = false`.

The expected next step is a backing-cell/reference audit plus constant-offset follow-through for any nontransparent `sharedManager()` result uses reported by the real v39 run.

## Reproduce

If v38 already exists locally:

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only
bash tools/ml/run_nevergone_beaten_script_engine_manager_v39.sh
```

If v38 must be regenerated first:

```bash
bash tools/ml/run_nevergone_beaten_script_engine_manager_v39.sh \
  /home/pes/ghidra_projects/nevergone/com.hippiegame.nevergone.apk
```

Primary outputs:

```text
$HOME/.cache/nevergone-recomp/receiver-provenance/beaten-script-engine-manager-v39.json
$HOME/.cache/nevergone-recomp/receiver-provenance/beaten-script-engine-manager-v39.txt
$HOME/nevergone-beaten-script-engine-manager-v39.tar.gz
```
