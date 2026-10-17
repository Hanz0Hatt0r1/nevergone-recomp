# Manual reconstruction: player health clamps and hit-reaction state gate

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## RevEngInstr selection

| Function | Entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `MEPlayer::setHealth` | `0x0032bf0a` | 30 | 2 | 3 | 1 | 3 |
| `MEPlayer::setHealth_T` | `0x0032bf4e` | 42 | 2 | 3 | 2 | 3 |
| `MEPlayer::isBeatenState` | `0x0032c7b4` | 17 | 0 | 3 | 1 | 0 |

All three RevEngInstr rows are complete. Their operation counts ranked them as
bounded candidates; the exact field and range behavior below comes from the
original ARMv7 instructions.

## Health setter behavior

Both health setters convert the signed maximum at `MEPlayer +0x11c` to `float`
and store the resulting health at `MEPlayer +0x16c`. The float setter at
`0x0032bf0a` stores its request first. The integer variant at `0x0032bf4e`
converts its request with `vcvt.f32.s32` before applying the same clamp.

The branches implement this order:

1. If requested health is greater than the converted maximum, use the maximum.
2. Otherwise, if requested health is less than zero, use zero.
3. Otherwise, keep the request.

The comparisons preserve negative zero and NaN as the native ARM path does.
`setHealth_T` then uses `vcvt.u32.f32` on the clamped value and passes that
integer to the team health-bar callback. `player_health_contract::set_team_health`
returns that value only when it is finite and in the uint32 conversion range;
it fails closed outside that domain.

The companion `player_health_contract::set_health` and
`set_team_health` helpers reproduce the numeric state updates. They do not
claim that the port's health-bar UI or the larger player combat flow is wired
to these contracts yet.

## Hit-reaction state gate

`MEPlayer::isBeatenState()` loads the state at `MEPlayer +0x278`. Thumb uses
unsigned subtract/compare branches, which accept exactly:

- `0x22..0x23`
- `0x28..0x2d`
- `0x3d..0x3e`
- `0x41`

The helper `player_health_contract::is_beaten_state()` implements these ranges.
This bounded predicate is distinct from the 17,737-operation
`MEPlayer::isBeaten()` state machine; it does not restore guard, dodge, parry,
damage, knockback, or hit effects.

Gold subsystem and function-role labels for these three entries are recorded
in `player-health-and-hit-reaction-labels.jsonl`. Adding those labels to the
previous manual-restoration v11 labels and rebuilding the local dataset as v12
produced 9,134 labelled functions, 6,847 structural groups, and 9,689 gold
label rows. The generated corpus and dataset remain outside Git.
