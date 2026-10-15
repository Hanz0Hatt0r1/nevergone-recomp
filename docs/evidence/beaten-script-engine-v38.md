# BeatenEffect script-engine reachability v38

v37 joined the three BeatenEffect child-lifecycle script-engine calls with the exact packaged static setter audit. It correctly kept the channel open because its single `dynamic_symbol_lookup_or_runtime_injection_excluded` field combined two different questions.

v38 narrows that boundary without promoting any receiver class.

## Proven packaged boundary

The existing exact-APK `ControlsDlsymClosure/1` audit proves the complete direct `dlsym` call surface for the shipped ARMv7 binary and the second packaged native library. It establishes that:

- all seven direct `dlsym` calls are accounted for;
- the bounded dynamic lookup strings do not contain `setScriptEngine` or `CCScriptEngineManager`;
- neither generic OpenSL lookup helper is retained as a packaged function pointer;
- `libffmpeg.so` does not reference the Cocos script-engine setter or the generic lookup helpers;
- packaged `dlsym` and packaged cross-library lookup of `CCScriptEngineManager::setScriptEngine` are excluded.

Combined with v37, the packaged application starts with a null manager engine pointer, has no packaged static assignment to that pointer, and has no packaged dynamic lookup path to the setter.

## Remaining open frontier

This is not a whole-runtime proof. v38 deliberately leaves these channels open:

1. external or injected runtime code that is not part of the packaged static native surface;
2. an as-yet-unbounded runtime alias write to the manager's engine slot.

Therefore the three child lifecycle script-engine calls are still not proven unreachable at runtime, script side effects are not closed, and no BeatenEffect receiver class is promoted.

## Run

The runner reuses v37 and the existing packaged dlsym audit:

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only

bash tools/ml/run_nevergone_beaten_script_engine_v38.sh \
  /home/pes/ghidra_projects/nevergone/com.hippiegame.nevergone.apk
```

The v37 prerequisites still apply, including the v36 child-lifecycle evidence in the receiver-provenance work directory.

Output:

- `~/.cache/nevergone-recomp/receiver-provenance/beaten-script-engine-v38.json`
- `~/.cache/nevergone-recomp/receiver-provenance/beaten-script-engine-v38.txt`
- `~/nevergone-beaten-script-engine-v38.tar.gz`

The archive also carries the v37 and dlsym prerequisite reports so the packaged/runtime boundary remains auditable.

`affects_routing=false`.
