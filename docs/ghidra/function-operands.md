# Focused numeric operand metadata

`tools/ghidra/ExportFunctionOperands.java` is a small clean-room Ghidra post-script for cases where the broad checked-in metadata index proves control flow but does not contain the numeric constants needed to reproduce behavior, such as `CCPoint` coordinates or action durations.

It exports metadata for exactly one analyzed function. Rows may contain:

- immediate scalar operands recognized by Ghidra;
- numeric `Data` values referenced directly by an instruction;
- numeric `Data` values reached through non-call references such as literal pools.

The TSV intentionally omits instruction bytes, mnemonics/disassembly text, decompiler output, strings, and surrounding binary contents.

## Usage

Run the script against the user's analyzed program in a writable/copy project:

```text
ExportFunctionOperands.java <function-address-or-name> <output.tsv>
```

For example, to investigate the unresolved `SingleSelectHero::initUI()` placement constants:

```text
ExportFunctionOperands.java 00438c20 /tmp/single-select-initui-operands.tsv
```

The output columns are:

```text
function_entry  instruction  operand  kind  bit_length  signed_value  unsigned_hex  target  target_type  number_value
```

`kind=immediate` records an instruction scalar. `kind=referenced-scalar` or `referenced-number` records a numeric value defined at a referenced address. The latter is useful on ARM/Thumb code that loads floating-point or integer constants through a literal pool.

This focused export should be used as evidence only. A numeric value still needs to be associated with the correct reconstructed call/field before it is implemented.
