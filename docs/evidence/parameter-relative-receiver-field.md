# Parameter-relative receiver field probe

This evidence-only stage targets the one non-quarantined `receiver_not_single_pointer` row remaining after v12:

- `libcocos2dcpp.so:002a647e` call `4`, dispatch slot `0x58`.

The prior structural provenance already established that the canonical receiver is a `LOAD` whose address root reaches parameter `0`, with no direct caller calls available. That does **not** prove the receiver is `this`; it is compatible with a field load from the current object.

`audit_nevergone_parameter_relative_receiver_field.py` therefore consumes the persisted structural slice and folds only the receiver LOAD address subtree into an affine `parameter0 + constant_offset` form. The parameter root token must match the independent structural-provenance row exactly.

The audit also resolves the current function entry against the native `game.tsv` symbol index using the project image delta (`Ghidra = ELF + 0x10000`) and reverse-maps the function entry through `StaticVtableSlotCatalog/3`. Native method ownership and vtable membership are reported only as independent context for the next field-write provenance stage; neither is used to infer the loaded field's class.

Possible result states:

- `parameter_relative_field_with_virtual_owner_evidence`: exact field offset folded, native owner unique, and that owner has an exact vtable entry targeting the current function.
- `parameter_relative_field_symbol_owner_only`: exact field offset and native owner are known, but virtual-owner membership is not independently present in the catalog.
- `parameter_relative_field_evidence_incomplete`: any structural, symbol, or affine-fold identity gate failed.

No receiver query is emitted and routing is unchanged.

Run:

```bash
cd ~/nevergone-recomp
git fetch origin
git switch -C probe-parameter-relative-receiver-field origin/probe-parameter-relative-receiver-field
bash tools/ml/run_nevergone_parameter_relative_receiver_field_probe.sh
```

Outputs are written under `$HOME/.cache/nevergone-recomp/receiver-provenance/` as `parameter-relative-receiver-field-audit.jsonl` and `.txt`.
