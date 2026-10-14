# P1A: Controls alias operand roots (15-site follow-up)

The prior Controls alias triage isolated 15 bounded follow-up sites:
9 `STORE` operations whose address base is defined by `LOAD`,
3 `CALLIND` operations, and 3 `ControlsButton::create` calls that
receive a parameter-zero-derived argument.

`ExportControlsAliasOperandRoots.java` is a read-only Ghidra exporter
for these exact operation addresses and high-P-code sequence times.
It traces each selected operand through transparent operations and
constant pointer arithmetic, retaining a conservative root token,
base offset, high-P-code parameter identity (if available), the
first `LOAD` address and a nested `LOAD` address. It reports
`missing`, `opcode_mismatch` or `operand_shape_mismatch` instead of
silently omitting unexpected sites. The script does not modify
types, instructions, source code, or dispatch routing.

The Python auditor requires all 15 exact sites and their operand
rows, checks `STORE` address/value, `CALLIND` target and factory
callback argument presence, and reports recovered root/LOAD offsets.
A structurally complete result **does not** prove absence of
aliases or unique receiver types. Every output retains
`all_alias_writers_excluded=false`, `receiver_class_proven=false`,
`affects_routing=false`.

## Run on Manjaro

The original `nevergone-controls-field-alias-evidence.tar.gz` and
the same original APK are required. Ghidra must have the existing
`libcocos2dcpp.so` project imported.

```bash
cd `/nevergone-recomp
git switch main
git pull --ff-only

bash tools/ml/run_nevergone_controls_alias_operand_probe.sh \
  `/nevergone-controls-field-alias-evidence.tar.gz \
  /path/to/com.hippiegame.nevergone.apk
```

The runner revalidates the original archive against the exact ELF,
generates the 15-site plan, invokes Ghidra headlessly and audits the
result. It produces:

`$HOME/nevergone-controls-alias-operand-roots.tar.gz`

The archive contains the plan, prior triage summary, raw Ghidra
operand-root TSV and the audit JSONL/text. It excludes the APK,
ELF, assets and decompiler C text. Upload it for the next
receiver/field-alias recovery step. A nonzero audit exit still
produces the archive; a Ghidra script failure may require fixing
the exporter before an archive can be generated.
