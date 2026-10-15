# Residual receiver closure v24

## Why v24 exists

The first real v23 run exposed a namespace-sensitive owner-detection bug in `ExportClassOwnerAliasFlow.java`. Ghidra reports the implicit `this` parameter for `PVEDATA::equipToolsListMsg` as `equipToolsListMsg *`, while the exporter compared it against the fully qualified `PVEDATA::equipToolsListMsg *`. As a result, all 28 protobuf methods were incorrectly treated as non-owner methods and the protobuf alias frontier appeared closed without actually being analyzed.

v24 fixes that fail-open condition. A short unqualified class type is accepted only when the function is already owned by the exact requested class and parameter zero is explicitly named `this`/`__this`. Full qualified type matches retain their previous fast path.

## Targeted evidence

The v23 data already reduced the receiver problem to a finite set. v24 therefore avoids another broad scan and reuses existing Ghidra exporters.

`ExportFunctionPcodeFlow.java` is run for thirteen functions:

- protobuf writers: `00299a66`, `00299a88`, `00299aaa`, `00299acc`, `00299fae`, `0029c20c`;
- `BeatenEffect` blocker functions: `002a3908`, `002a3e94`, `002a474c`, `002a47c6`;
- `CCJoyStick` blocker functions: `002a5f00`, `002a6284`, `002a6584`.

`ExportIndirectCallStructuralReceiverSlice.java` is additionally run on the seven Cocos blocker functions to recover exact CALLIND slot structure rather than treating every indirect owner use as one opaque frontier.

The corrected protobuf alias flow is regenerated, while the already valid v23 `BeatenEffect` and `CCJoyStick` flow files are reused.

## Protobuf native writers

The real v23 field-write inventory already shows four parallel field groups at `+0x8/+0xc/+0x10/+0x14`:

- `mutable_titem1..4` store `operator.new` returns whose Ghidra value type is `equipToolsDataMsg *`;
- `SharedCtor` stores null;
- `InitAsDefaultInstance` stores `PVEDATA::equipToolsDataMsg::default_instance`;
- `Swap` transfers values through LOAD roots.

v24 records whether each mutable accessor's P-code also contains an exact `PVEDATA::equipToolsDataMsg::equipToolsDataMsg` constructor call. It does **not** promote the class solely from the recovered `.proto` schema or from a typed `operator.new` result.

## Fail-closed contract

`audit_nevergone_residual_closure_v24.py` is evidence-only. It reports corrected owner-method counts, remaining whole-owner/unknown/address-escape blockers, protobuf writer patterns, exact direct-call metadata, and CALLIND slot summaries. It never emits receiver queries and all output carries `affects_routing=false`.

The runner installs an EXIT trap that packages whatever evidence exists even if one Ghidra probe fails, so a single failed function no longer discards the rest of the diagnostic run.

## Run

```bash
bash tools/ml/run_nevergone_residual_receiver_closure_v24.sh
```

Archive:

```text
~/nevergone-residual-receiver-closure-v24.tar.gz
```
