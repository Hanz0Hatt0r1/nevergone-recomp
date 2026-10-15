# Tutorial enemy spawn-plan contract

This note records the clean-room geometry and timing contract recovered from the
original ARMv7 `LessonLayer::nextLesson()` and
`LessonLayer::nextLesson(MEPLAYERSTATE)` tutorial branches. It contains no
original enemy assets, action data or proprietary binary bytes.

## Inputs retained by the original code

The relevant branches consume:

- the current player position;
- `MEPlayer::isFace()`;
- the player's current `PLAYERFACEDIR` field, forwarded unchanged to enemy
  creation;
- three unsigned scene-limit fields reached through
  `GameScene +0x238 -> +0x20`: two vertical edges and one right-side X edge.

The project contract names those raw scene values `vertical_edge_a`,
`vertical_edge_b`, and `right_edge_x` rather than assigning semantic map names
that have not yet been independently proven.

## Horizontal side selection

For the ordinary tutorial waves and the single block enemy, the shipped code
first reserves 500 design units of horizontal room.

When `MEPlayer::isFace()` is false, positive X is used while:

```text
player_x <= right_edge_x - 500
```

otherwise the spawn side flips to negative X.

When `MEPlayer::isFace()` is true, positive X is used only while:

```text
player_x < 500
```

and negative X is used at or beyond 500. This preserves the original edge
fallback rather than assuming that `isFace()` alone is the desired spawn side.

The later air-attack target is different: it is placed exactly 600 units from
the player, using positive X when `isFace()==false` and negative X when
`isFace()==true`, without that 500-unit fallback branch.

## Attack wave after Jump

After the recovered Jump state gate (`MEPLAYERSTATE == 0x20`), the original
creates three enemy ID `3`, state `0x1c` requests:

```text
1: x = player_x +/- 200, immediate
2: x = player_x +/- 290, delay 1.0 s
3: x = player_x +/- 450, delay 0.5 s
```

The sign comes from the horizontal side selection above. All requests forward
the player's recovered face-direction value.

Before the second enemy is added, the original also calls
`GameScene::falingRook_Enemy()` at:

```text
x = player_x +/- 290
y = player_y - 120
```

The delayed enemy Y offsets depend on proximity to the two raw vertical scene
edges. The comparison threshold is exactly 20 design units:

| Condition | Enemy 2 Y | Enemy 3 Y |
| --- | ---: | ---: |
| `abs(vertical_edge_b - player_y) < 20` | `player_y - 30` | `player_y - 10` |
| else if `abs(vertical_edge_a - player_y) < 20` | `player_y + 20` | `player_y + 10` |
| otherwise | `player_y - 20` | `player_y + 20` |

The immediate first enemy stays at `player_y`.

## Block enemy after the Attack objective

Once the recovered Attack world-state gate is satisfied, the original creates
one enemy ID `3`, state `0x2f`, immediately at:

```text
x = player_x +/- 160
y = player_y
```

It uses the same 500-unit horizontal side-selection/fallback contract as the
three-enemy waves.

## Air-attack target after Dodge

After one of the recovered Dodge states (`0x11`, `0x17`, `0x2f`, `0x30`) the
original creates one enemy:

```text
enemy id    = 0x15
state       = 0x14
x           = player_x + 600  when isFace()==false
            = player_x - 600  when isFace()==true
y           = player_y
```

The same branch grants the tutorial item used by the later Rage-fill objective;
item ownership remains outside this geometry-only contract.

## Execution wave after the Air Attack objective

After the air-attack target is dead, the later three-enemy wave reuses the same
X offsets and vertical-edge correction table as the first wave:

```text
1: x = player_x +/- 200, immediate
2: x = player_x +/- 290, delay 1.2 s
3: x = player_x +/- 450, delay 0.5 s
```

All three are again enemy ID `3`, state `0x1c`. Unlike the earlier attack wave,
this branch does not call `falingRook_Enemy()` before the delayed entries.

The `1.2` second delay is an original floating-point literal and differs from
the first wave's `1.0` second delay.

## Remaining live-runtime boundary

`tutorial_enemy_spawn_plan` is deliberately side-effect free. It converts
recovered player/scene inputs into deterministic enemy/effect requests; it does
not yet instantiate `EnemyObject`, simulate AI, assign HP, process hits, or
infer death from button presses.

The next live combat slice can consume these requests into a project-owned
enemy roster. That roster must only report the tutorial's zero-live-enemy gates
from explicit EnemyObject/death state, preserving the already recovered
`BattleManager::enemyNullCount()` semantics.
