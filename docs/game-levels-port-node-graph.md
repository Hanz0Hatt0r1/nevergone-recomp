# GameLevels LinkScenePortNode graph evidence

This note records the clean-room linking behavior recovered from Never Gone 1.0.9 ARMv7 `GameLevels::LinkScenePortNode()`.

## Original behavior

The shipped function begins at disassembly address `0x002c2024`. It iterates the retained port-node array with a nested loop. The current node is never compared with itself.

For each source node, the function compares the source's third serialized string (the field populated at object offset `+0x1c` by `LoadGL_PortNode()`) with each candidate's first serialized string (object offset `+0x14`). On the first equality it:

1. stores the candidate as the source forward link and marks the source linked;
2. stores the source as the candidate reverse link and marks the candidate reverse-linked;
3. exits the inner loop and continues with the next source.

Because the reverse pointer is assigned unconditionally, a later source that resolves to the same candidate overwrites that candidate's earlier reverse link. The earlier source's forward link remains intact.

## Clean-room graph

`game_levels_port_node_graph::link()` represents the recovered pointer relationships as array indices and explicit booleans. It preserves source order, candidate order, the self-skip, first-match rule, and reverse-link overwrite behavior without depending on original object layouts or pointers.

The focused smoke covers an empty graph, a normal forward/reverse pair, self-only rejection, duplicate target first-match behavior, reverse overwrite by a later source, and missing targets.

## Next step

Feed this graph from the fully parsed `LoadGL_PortNode()` result in the GameLevels runtime path. The next evidence target is the scene/port lookup used when entering the first offline scene; scene selection must remain separate from graph framing until its original control flow is proven.
