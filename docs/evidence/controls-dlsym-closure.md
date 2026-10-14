# P1A: packaged dlsym closure for Controls script-engine setter

PR #588 established that the packaged static application has no direct branch, PROGBITS function pointer, DEX reference or linked CCLuaEngine path to `CCScriptEngineManager::setScriptEngine`. The remaining packaged dynamic-lookup question is `dlsym`.

The exact original ARMv7 ELF imports `dlsym`. Its `R_ARM_JUMP_SLOT` resolves to PLT `0x278ee8`. A complete halfword-aligned Thumb BLX-immediate scan of `.text` finds exactly **7** calls to that PLT entry.

Those seven sites are bounded by exact function hashes and fixed caller surfaces:

- `getInterfaceID(char const*)`: one `dlsym` internally; exactly **8** direct callers, all from the OpenSL audio implementation and using `SL_IID_SEEK`, `SL_IID_MUTESOLO`, `SL_IID_VOLUME`, `SL_IID_PLAY`, `SL_IID_ENGINE`, or `SL_IID_ENVIRONMENTALREVERB`.
- `getFuncPtr(char const*)`: one `dlsym` internally; exactly **1** direct caller, passing `slCreateEngine`.
- `getFileDescriptor(...)`: four direct `dlsym` calls for `AAssetManager_fromJava`, `AAssetManager_open`, `AAsset_openFileDescriptor`, and `AAsset_close`.
- one bundled compatibility helper opens `libc.so` and resolves `getauxval`.

Neither generic OpenSL helper is retained as a PROGBITS function pointer. The second packaged native library, exact `libffmpeg.so` SHA-256 `f5e8392d27b182671be5070bc7aa5772a8b19711af222833651faf8516eacdde`, contains no `setScriptEngine`, `CCScriptEngineManager`, `getFuncPtr`, or `getInterfaceID` reference.

Therefore the complete packaged native `dlsym` surface contains no lookup of the Cocos script-engine setter. This closes the **packaged dlsym** variant of the residual touch/script channel. It does not exclude externally injected code, hooks, corruption, a debugger, or code loaded outside the packaged application.

Reproduce:

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only
bash tools/ml/run_nevergone_controls_dlsym_closure.sh \
  /path/to/com.hippiegame.nevergone.apk
```

Safety remains `whole_program_alias_writes_excluded=false`, `receiver_class_proven=false`, `affects_routing=false`; no production receiver-routing change is authorized by this audit alone.
