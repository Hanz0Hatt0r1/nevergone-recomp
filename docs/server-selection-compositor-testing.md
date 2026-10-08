# Server selection compositor verification

The reconstructed server-selection overlay is intentionally split into three layers:

1. `server_selection_state` — recovered selection/confirm semantics;
2. `server_selection_layout` — recovered `NewServerList` row placement/hit rectangles;
3. `server_selection_view` / `server_selection_compositor` — surface mapping plus project-owned fallback drawing while the original `ServerList` artwork is unavailable.

## Automated checks

`tools/server_selection_view_smoke.cpp` verifies:

- exact 1136x640 surface mapping;
- aspect-fit mapping on 1920x1080;
- letterbox rejection on portrait surfaces;
- Android top-left Y conversion into the recovered bottom-left design canvas;
- row hit testing through the same surface mapping used by the compositor;
- the fallback confirm rectangle retaining recovered logical tag `10002`.

The Android build is also required because the compositor itself depends on GLES2/JNI and is linked into both configured Android ABIs. The existing 16 KiB check remains the final packaging gate.

## Manual runtime checks

When a structured `cpp_OnGetServerList` callback drives the Management route to `server-selection`:

- the SingleLogin scene remains behind the overlay;
- fallback rows appear only while a valid server payload exists;
- `LastLoginServer` is highlighted when it exactly matches an id;
- tapping another visible row changes the highlight;
- vertical movement greater than 10 design pixels does not change selection;
- the fallback confirm control is present only after selection;
- confirming sets a pending `EnterRequest(ip,id)` but does not yet fabricate a persistent Lua dispatch.

The runtime diagnostics identify the visuals as `project-owned fallback`. Once user-imported update/OBB content supplies the original row/button assets, only the visual resource/dimension layer should change.
