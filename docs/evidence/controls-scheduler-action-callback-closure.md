# P1A: Controls scheduler/action callback closure (2026-10-14)

## Scope

After exact native argument-offset recovery, 35 calls receive the unadjusted
`Controls*`. The retained `ControlsButton` and joystick-delegate paths were
already closed separately. This note covers the remaining callback mechanisms
that can retain `Controls*` beyond a direct call:

- two `CCNode::scheduleOnce(...)` calls;
- one persistent `CCNode::schedule(...)` call;
- one `CCCallFuncN::create(...)` action callback.

All checks use the original ARMv7 library SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.
No proprietary binary or disassembly listing is committed.

## Scheduler callbacks

The two combo paths use an exact relocated GOT pointer to
`Controls::comboEnd(float)`, while `Controls::init()` schedules
`Controls::tick(float)` through a second exact relocated GOT pointer.
The call windows pass the unadjusted `Controls*` as the node/target.

The original Cocos scheduler wrappers are SHA-pinned through the overload
that forwards the member-function pointer into the scheduling core. The core
forwards the node as the scheduler target (`r3`). Thus the scheduled callbacks
retain the `Controls` target semantically, rather than merely receiving it
transiently at setup time.

The exact callback bodies are also SHA-pinned. Direct immediate stores are:

- `comboEnd(float)`: none;
- `tick(float)`: `Controls+0x154`, `+0x110`, `+0x118`, `+0x10c`.

Neither callback directly writes the retained object fields
`+0x124`, `+0x12c`, `+0x130`, `+0x134`, or `+0x174`.

## CCCallFuncN action callback

`Controls::showAToJTip()` passes the unadjusted `Controls*` as the
`CCCallFuncN` target. Original Cocos code is pinned end-to-end:

1. `CCCallFunc::initWithTarget()` retains the target and stores it at
   action offset `+0x24`;
2. `CCCallFuncN::initWithTarget()` stores the ARM EABI member-function pair
   at `+0x2c/+0x30`;
3. `CCCallFuncN::execute()` reloads the target/member pointer and dispatches
   it through `BLX`.

The retained member callback is exactly
`Controls::showKillTipsCallback(CCNode*)`. Its 12-byte native body performs
no direct STORE; it executes one virtual call through `Controls` slot `0xd8`.
The exact relocated `_ZTV8Controls` entry resolves to inherited
`cocos2d::CCNode::getScheduler()`. That six-byte function only loads the
scheduler pointer and returns.

Therefore this retained action callback also does not directly replace the
joystick/action-button pointer fields.

## Reproduce

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only

bash tools/ml/run_nevergone_controls_scheduler_action_callbacks.sh \
  /path/to/com.hippiegame.nevergone.apk
```

Outputs are written under
`$HOME/.cache/nevergone-recomp/receiver-provenance/`:

- `controls-scheduler-action-callback-closure.jsonl`;
- `controls-scheduler-action-callback-closure.txt`.

## Conservative conclusion

The scheduled `comboEnd`/`tick` paths and the retained `CCCallFuncN`
`showKillTipsCallback` path are now exact and do not directly write the
retained Controls pointer fields. This removes the remaining known
scheduler/action callback retention mechanisms from the pointer-field alias
surface.

It still does not establish whole-program alias completeness. In particular,
ordinary Cocos base calls such as `CCNode::addChild` can establish parent
back-references, and touch-dispatch registration can retain an object for
later virtual callbacks. Those paths require their own bounded audit before
receiver classes are promoted.

Safety flags remain:

- `retained_scheduler_action_paths_closed=true`;
- `callbacks_directly_write_controls_pointer_fields=false`;
- `whole_program_alias_writes_excluded=false`;
- `receiver_class_proven=false`;
- `affects_routing=false`.
