Server-selection compositor PR summary:

- GLES overlay gated by the reconstructed server-selection route;
- recovered row geometry and selection state drive fallback visuals;
- Android surface coordinates are mapped into the 1136x640 design canvas;
- server-selection touch input is consumed before the legacy TapToStart path;
- confirm produces the existing pending EnterRequest without fabricating Lua dispatch;
- original ServerList artwork remains optional user-supplied content.
