# Server selection compositor scope

This compositor is a clean-room bridge between recovered server-selection behavior and the modern GLES2 runtime. It does not claim pixel-identical reconstruction while the original `gamescene_ui/ServerList/*` artwork is unavailable.

Recovered semantics retained by the runtime:

- structured server `id/name/ip` payloads;
- exact `LastLoginServer` preselection;
- two-column `NewServerList` row placement;
- full-row hit rectangles;
- `<=10` design-pixel row-tap threshold;
- confirm tag `10002` semantics;
- `EnterGameLogicServer(ip,id)` request shape.

Project-owned fallback details are intentionally isolated in `server_selection_view` and `server_selection_compositor`: row dimensions, confirm rectangle, and colors. They can be replaced later without changing the recovered state/layout contract.
