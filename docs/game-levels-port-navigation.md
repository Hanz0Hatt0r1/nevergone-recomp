# GameLevels port navigation evidence

This note records the clean-room behavior recovered from Never Gone 1.0.9 ARMv7 `GameLevels::GetPortNodeLinkPortNode(EVENT_PORT_TYPE)` at disassembly address `0x002c2538`.

## Recovered transition behavior

The function first reads the current port-node pointer. If it is null, the function returns null without changing the stored event-port type.

For requested event type `0`, the original:

1. stores event-port type `1` in `GameLevels +0x40`;
2. reads the current node's reverse-link pointer at object offset `+0x2c`;
3. replaces the GameLevels current-port pointer with that value, including null;
4. returns the new current pointer.

For requested event type `1`, it performs the symmetric operation:

1. stores event-port type `0`;
2. reads the current node's forward-link pointer at `+0x270`;
3. replaces current-port with that value, including null;
4. returns it.

Other requested values return null without changing current-port or the stored event-port type. The clean-room implementation keeps the numeric values structural; direction names are intentionally not invented.

`GameScene::GoToCheckPointScene(EVENT_PORT_TYPE)` at `0x00343dd0` calls this function before beginning its scene-switch/fade path, proving the recovered operation is a live scene-navigation boundary rather than unused metadata.

## Project-owned state

`game_levels_port_navigation::State` stores the current port-node index plus the recovered event-port type. `step()` applies the same transition rules against the index-based `game_levels_port_node_graph::Graph`, including clearing the current node when a supported link is missing.

## Next step

Retain this navigation state in `game_levels_model::Model`, initialize it from the recovered start-port selection, and resolve the scene GUID after each successful step so the first offline scene path can advance without original pointers.
