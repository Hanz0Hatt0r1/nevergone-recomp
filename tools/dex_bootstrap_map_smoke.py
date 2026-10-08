#!/usr/bin/env python3
from dex_bootstrap_map import instruction_width, invoke_registers

# const-string v0, string@1234; invoke-static {v0}, method@4321; return-void
units = [0x001A, 0x1234, 0x1071, 0x4321, 0x0000, 0x000E]
assert instruction_width(units, 0) == 2
assert instruction_width(units, 2) == 3
assert invoke_registers(units, 2) == [0]
assert instruction_width(units, 5) == 1

# invoke-static/range {v4}, method@1234
range_units = [0x0177, 0x1234, 0x0004]
assert instruction_width(range_units, 0) == 3
assert invoke_registers(range_units, 0) == [4]

# Payload widths must be skipped atomically by the instruction walker.
packed_switch = [0x0100, 0x0002, 0, 0, 0, 0, 0, 0]
assert instruction_width(packed_switch, 0) == 8
sparse_switch = [0x0200, 0x0002] + [0] * 8
assert instruction_width(sparse_switch, 0) == 10
fill_array = [0x0300, 0x0001, 0x0003, 0x0000, 0, 0]
assert instruction_width(fill_array, 0) == 6

print("dex bootstrap map smoke: ok")
