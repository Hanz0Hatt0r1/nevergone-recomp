# P1A: retained Controls alias callback closure (2026-10-14)

## Scope

This follow-up starts from the exact original ARMv7 `libcocos2dcpp.so`
SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.
It closes the two retained owner-pointer paths already identified by the
Controls field-alias work:

1. `ControlsButton+0x110` retains the callback object passed by
   `Controls::init()`;
2. `CCJoyStick+0x14c` retains the joystick delegate pointer installed by
   `Controls::init()`.

This is a **local retained-callback proof**, not a whole-program alias
exclusion. Production routing is unchanged.

## ControlsButton callback object

For all three action-button constructions, `Controls::init()` passes the
unadjusted `Controls*` as the callback object. The three exact
`R_ARM_RELATIVE` GOT entries resolve to:

- `Controls::menuABCallback()`;
- `Controls::menuDBCallback()`;
- `Controls::menuJCallback()`.

The ARM EABI member-function adjustment word is zero. `ControlsButton::create`
forwards the callback object, and `ControlsButton::initWithNormalImage()`
stores it at `ControlsButton+0x110`. The two-word member-function pointer is
stored at `+0x114/+0x118`. `ControlsButton::touchEnd()` reads those fields and
executes the retained member callback through `BLX`.

All three callback functions have the same eight-byte body and perform only:

```text
Controls+0x118 = 0
return
```

They do **not** write `Controls+0x124`, `+0x12c`, `+0x130`, `+0x134`, or
`+0x174`. Therefore the retained `ControlsButton` callback path does not
replace the joystick or action-button pointers.

## CCJoyStick delegate back-reference

The `Controls` constructor installs the secondary `CCJoyStickDelegate`
vtable address point into `Controls+0x104`. The address point is
`_ZTV8Controls + 0x244 = 0x8e46d4` and its first three relocated entries are:

| Delegate slot | Secondary-vtable target |
| --- | --- |
| `+0x0` | thunk `-0x104` -> `Controls::onCCJoyStickUpdate(...)` |
| `+0x4` | thunk `-0x104` -> `Controls::onCCJoyStickActivated(...)` |
| `+0x8` | thunk `-0x104` -> `Controls::onCCJoyStickDeactivated(...)` |

`Controls::init()` stores `Controls+0x104` into `CCJoyStick+0x14c`. An exact
scan of every symbolized `CCJoyStick` method finds only four immediate word
accesses to `+0x14c`:

- constructor: zero initialization;
- `timerUpdate`: load and dispatch slot `0`;
- `startTimer`: load and dispatch slot `4`;
- `stopTimer`: load and dispatch slot `8`.

The three target method bodies are SHA-pinned. Their direct stores are:

- `onCCJoyStickUpdate`: `Controls+0x10c`;
- `onCCJoyStickActivated`: `Controls+0x15c`, `+0x160`;
- `onCCJoyStickDeactivated`: `Controls+0x10c`, `+0x11c`, `+0x151`,
  `+0x15c`, `+0x158`, `+0x160`, `+0x110`.

None of these stores targets `Controls+0x124`, `+0x12c`, `+0x130`, `+0x134`
or `+0x174`. Thus the retained joystick-delegate path also does not directly
replace the joystick/action-button pointers.

## Reproduce

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only

bash tools/ml/run_nevergone_controls_retained_alias_closure.sh \
  /path/to/com.hippiegame.nevergone.apk
```

Output:

- `$HOME/.cache/nevergone-recomp/receiver-provenance/controls-retained-alias-closure.jsonl`
- `$HOME/.cache/nevergone-recomp/receiver-provenance/controls-retained-alias-closure.txt`

The checker validates the exact ELF SHA, dynamic symbols, callback GOT
relocations, secondary Controls vtable relocations, callback-object storage,
member-function dispatch, all `CCJoyStick+0x14c` immediate accesses and the
exact delegate target bodies.

## Conservative conclusion

Both **known retained owner-pointer callback channels** have exact targets and
neither directly writes the retained joystick/action-button pointer fields.
This materially reduces the alias surface, but other functions can still
receive `Controls*` or pointers derived from it. Therefore:

- `local_retained_alias_paths_closed=true`;
- `whole_program_alias_writes_excluded=false`;
- `receiver_class_proven=false`;
- `affects_routing=false`.
