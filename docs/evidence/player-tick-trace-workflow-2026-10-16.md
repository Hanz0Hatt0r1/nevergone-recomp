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

Live parity check on the Redmi Note 8 Pro, 2026-10-16: two rightward swipes
were sent as separate DOWN/MOVE/UP events with 50 ms spacing so the GL update
thread could sample the stick between events. The first swipe held normal
direction state `0x13`; after release, the next update of the second swipe
latched direction state `0x11` and movement action `0x1`. Fast reactivation
count became 1, control state remained `0x13`, and the run lesson advanced to
jump. During that fast update, the player offset changed from x `234.275` to
`334.275` across five applied updates. This matches the original capture's
frame-195 `0x11`/action-`0x1`/`movePlayer` transition. The prior single-command
ADB swipe queued its move events between GL samples and could not reproduce the
original tick ordering.

Touch coordinates depend on the diagnostic panel's layout. With the panel
hidden, the GL surface measured `2264x902` and the stick center mapped to screen
`(483,658)`; with the panel open, the surface measured `2264x541` and the same
center mapped to `(483,395)`. Perform gameplay gestures with the panel hidden,
then open and refresh diagnostics to read the retained trace.

Save a runtime diagnostics dump with the existing Android evidence workflow,
then extract the recomp side to CSV:

```sh
python3 tools/extract_player_tick_trace.py \
  evidence/android-player-input-runtime/after-diagnostics.txt \
  --output /tmp/nevergone-recomp-player-ticks.csv
```

Touch events and update samples share a monotonically increasing sequence
number. The trace keeps 128 pre-input update rows, then records the input and
the next 384 updates before freezing. That preserves the causal window while
the diagnostics panel is opened. A new gesture starts a fresh window; the
512-row cap also keeps the report within Android's accessibility text limit.

The 2026-10-16 device run found and fixed a race in TapToStart: the touch phase
was published before its pending auto-login request, allowing the GL thread to
miss the request. The state is now updated under one mutex. On the Redmi Note 8
Pro, a clean launch reached `game-levels` after one accepted touch; the bounded
trace retained 512 rows in the first verification run. The input
runtime proof recorded 36 joystick moves, control state `0x13`, and seven
applied movement steps (x offset changed from `-125.724` to `-69.724`). The
trace now freezes after 384 post-input updates so the touch rows remain
available while diagnostics are collected.

`tools/tap_to_start_state_smoke.cpp` also stress-tests 20,000 concurrent
input-thread/GL-thread handoffs. The GL-side observer waits for phase 5 and
consumes the request exactly once; the test fails if it sees phase 5 without a
pending request.
