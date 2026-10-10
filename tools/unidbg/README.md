# Nevergone ARM32 unidbg harness

Local native-analysis tool. Original libraries are read only. Generated files, phone libraries and ADB stay ignored. See [observations](../../docs/evidence/unidbg-2026-10-10/README.md).

## Prerequisites

Linux, Java 17, Maven, Python 3.10+, binutils (`readelf`, `c++filt`), optional authorized Android USB device and `adb`.

Tested unidbg revision: `2ba8156eef31501ebd35194adb5795ef3e01a285`, version `0.9.10-SNAPSHOT`. Install that revision locally; it contains the MCP debugger. The idempotent `prepare_unidbg.py` adds the three explicit Java `Module` imports used in the tested local build. In the unidbg checkout:

```sh
python3 /path/to/nevergone-recomp/tools/unidbg/prepare_unidbg.py .
mvn -DskipTests -pl unidbg-android,backend/unicorn2 -am install
```

## Libraries and launch

From this directory:

```sh
export NEVERGONE_LIB_DIR=/path/to/armeabi-v7a
# Directory must contain libcocos2dcpp.so, libffmpeg.so, libGLESv2.so,
# libEGL.so and libcutils.so. Use matching ARM32 platform libraries.
python3 inventory.py
# Optional: collect dependencies from the authorized source device.
python3 pull_device_deps.py
./run.sh --smoke
./run.sh
```

At the debugger prompt enter `mcp 9239`. Connect the plugin to `http://localhost:9239/sse`. The harness stays attached until interrupted. `NEVERGONE_DEVICE_LIB_DIR` and `NEVERGONE_DEVICE_RUNTIME_DIR` override the default local `device-libs/` and `device-runtime/` directories. `ADB` overrides the executable used by the collector. The collector writes private device identification to ignored `data/`; publish only sanitized metadata.

Default runtime: five copied device libraries (libc, libm, libdl, libc++, liblog); remaining bundled dependencies can still come from unidbg SDK23. This is a mixed emulated environment, not Android boot. `--sdk23-runtime` is an experimental fallback; it is not the environment validated here.

Constructors are disabled before loading. JNI_OnLoad runs explicitly; `--no-jni-onload` disables it. No Java UI, GL context, asset manager, application object graph or Lua state is established by this loader.

## Probes and verification

```sh
./run.sh --probe > target/native-probe.log 2>&1
python3 verify_evidence.py target/native-probe.log
python3 -m unittest discover -s . -p 'test_*.py'
```

`--probe` invokes the original exported decoder and two CCRect methods with synthetic data. Inspect the log for emulator faults as well as running the comparison: unidbg may swallow a native fault and return a register value. `--probe-egl` is an isolated diagnostic, currently expected to fail; do not treat that returned register as a successful EGL result.

`relr_plan.py` decodes ELF32 DT_RELR into a local relocation plan. Before JNI_OnLoad, the harness checks source SHA-256 and target values and applies missing rebases in emulator memory only. Already rebased words are skipped. Unsupported ELF formats, non-file-backed targets and unexpected values fail closed. This compensates for missing DT_RELR support in the tested unidbg revision; it is not a general Android linker implementation. All RELR fixes happen after loading with constructors disabled. Do not enable constructors before these fixes.

Dependencies returned by the custom resolver route their own nested dependencies through it, avoiding SDK resource lookup silently overriding the selected phone runtime. Root-file siblings are resolved by unidbg's ElfLibraryFile from the original library directory.

## Address conventions

TSV `value_hex` includes the Thumb bit; `code_offset_hex` clears it. Neither is generally an ELF file offset. Runtime pointer = module base + symbol value; disassembly/breakpoint instruction address = base + code offset. Re-query module bases every session. `call_symbol` uses the mangled exported name and needs ABI-correct arguments and initialized object state. JNI nativeInit/nativeRender and AppDelegate are candidates for later environment reconstruction, not safe zero-argument calls.
