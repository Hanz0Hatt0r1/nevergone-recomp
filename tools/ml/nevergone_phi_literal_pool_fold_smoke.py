#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
from pathlib import Path

MODULE_PATH = Path(__file__).with_name("audit_nevergone_phi_secondary_load_trees.py")
spec = importlib.util.spec_from_file_location("phi_audit", MODULE_PATH)
assert spec and spec.loader
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def node(node_id: int, parent: int, input_index: int, opcode: str, token: str, const: str = "-") -> dict[str, str]:
    return {
        "node_id": str(node_id),
        "parent_node_id": str(parent),
        "input_index": str(input_index),
        "defining_opcode": opcode,
        "node_token": token,
        "constant_value": const,
    }


# INT_ADD(address:0x1000, const 4). When 0x1000 is proven literal-pool data,
# the address leaf contributes the cell contents (0x20), not its storage address.
nodes = {
    0: node(0, -1, -1, "INT_ADD", "def:INT_ADD@00002000:1:4"),
    1: node(1, 0, 0, "-", "address:00001000:4"),
    2: node(2, 0, 1, "-", "const:0x4:4", "0x4"),
}
children = mod.node_children(nodes)
unresolved: set[int] = set()
subs: set[int] = set()
value = mod.fold_node(0, nodes, children, {}, {0x1000: 0x20}, {}, unresolved, subs, set())
assert value == 0x24, hex(value or 0)
assert subs == {0x1000}, subs
assert not unresolved, unresolved

# Without literal-pool proof the same terminal remains a normal address constant.
unresolved = set()
subs = set()
value = mod.fold_node(0, nodes, children, {}, {}, {}, unresolved, subs, set())
assert value == 0x1004, hex(value or 0)
assert not subs, subs

# LOAD of the corrected expression must stop fail-closed when the resulting memory
# address has not itself been classified.
nodes2 = {
    0: node(0, -1, -1, "LOAD", "def:LOAD@00003000:1:4"),
    1: node(1, 0, 1, "INT_ADD", "def:INT_ADD@00002ffe:1:4"),
    2: node(2, 1, 0, "-", "address:00001000:4"),
    3: node(3, 1, 1, "-", "const:0x4:4", "0x4"),
}
children2 = mod.node_children(nodes2)
unresolved = set()
subs = set()
value = mod.fold_node(0, nodes2, children2, {}, {0x1000: 0x20}, {}, unresolved, subs, set())
assert value is None
assert unresolved == {0x24}, unresolved
assert subs == {0x1000}, subs

print("literal-pool folding smoke: pass")
