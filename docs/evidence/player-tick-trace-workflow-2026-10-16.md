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
context only.

On the retained original capture, the fast-run transition is visible on render
frame 195: dir-combo `0x11`, direction/control state `0x13`, one request for
action state `0x1`, and one `MEPlayer::movePlayer` call. Frame 196 keeps those
control values and continues movement without another action-state request.
This is direct hook observation for that run.

The recompilation retains a bounded ordered trace in runtime diagnostics: raw
touch events (`I`) and one state sample (`U`) after each gameplay-layer update.
Each `U` row records render-update number, fixed-clock tick,
joystick/combo state, player control/function state, applied movement steps,
position, and tutorial step. The fixed-clock tick can repeat across render
updates or advance by more than one during catch-up; the trace preserves that
fact instead of inventing intermediate states.

Save a runtime diagnostics dump with the existing Android evidence workflow,
then extract the recomp side to CSV:

```sh
python3 tools/extract_player_tick_trace.py \
  evidence/android-player-input-runtime/after-diagnostics.txt \
  --output /tmp/nevergone-recomp-player-ticks.csv
```

Touch events and update samples share a monotonically increasing sequence
number. At most 512 rows are retained, so capture diagnostics shortly after the
input sequence. The current device session is still at the startup route; the
gameplay input proof cannot run until the offline route reaches GameLevels.
