# Player update trace workflow

The original-device Frida capture records native call entry/exit events and
render-swap frame numbers. `tools/summarize_player_tick_trace.py` reduces its
JSONL output to a CSV row for each observed `GameScene::setDirComboState`
boundary, followed by the control setters, requested action-state arguments and
movement calls seen before the next boundary. The row includes the corresponding
render frame and elapsed capture time so the two clocks stay distinguishable.

Example:

```sh
python3 tools/summarize_player_tick_trace.py \
  evidence/original-runtime/nevertrace-005/native-trace.jsonl \
  --output /tmp/nevergone-original-player-ticks.csv
```

This boundary is an observed native call used as a proxy for one `GameScene`
update, based on the recovered `GameScene::tick()` setter order. It is not a
direct measurement of the original scheduler's `dt`, and its render-frame
number is not a tick number. An original/recomp comparison must align the same
input actions and compare each side's control state, action transition and
position at its own update boundary; wall-clock/frame counts are supporting
context only. Current recompilation diagnostics expose end-of-window movement
totals and state, not a corresponding per-update call trace yet, so this
workflow provides the original-side timeline and identifies the telemetry gap
to close next.

On the retained original capture, the fast-run transition is visible on render
frame 195: dir-combo `0x11`, direction/control state `0x13`, one request for
action state `0x1`, and one `MEPlayer::movePlayer` call. Frame 196 keeps those
control values and continues movement without another action-state request.
This is direct hook observation for that run; matching it to a new recomp trace
still requires per-update telemetry on the recomp side.
