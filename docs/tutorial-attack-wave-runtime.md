# First tutorial Attack-wave runtime

`tutorial_attack_wave_runtime` is the orchestration boundary for the first
combat lesson only. It intentionally does not generalize Block, Air Attack, or
Execution enemy lifecycles before those transitions have equivalent evidence.

## Start conditions

The runtime observes `tutorial_gameplay_state::Step::kAttack` and remains
blocked until both of these independent prerequisites are ready:

1. `tutorial_spawn_inputs::Result::ready()` is true for the current player,
   control generation, scene load attempt, and reviewed geometry;
2. every distinct enemy ID in the recovered Attack plan resolves to positive
   base HP through `tutorial_enemy_roster::BaseHealthResolver`.

The HP preflight happens **before** `Roster::begin()`. This matters because the
spawn plan is relative to the player's current position: if imported HP is
still unresolved, the player may continue moving and the later ready input
snapshot is the one used to build the wave. No stale geometry is frozen while
waiting for health evidence.

## Fixed-tick timing

After the wave begins, its elapsed time is derived only from the project's
recovered fixed logical clock:

```text
elapsed = (current_game_tick - start_game_tick) * (1 / 35 s)
```

For the first recovered Attack wave:

- enemy 1 is due at `0.0 s`;
- the `0.5 s` enemy becomes due on fixed tick 18 because tick 17 is still below
  0.5 seconds;
- the `1.0 s` enemy becomes due exactly on fixed tick 35.

A game-tick regression fails closed.

## Enemy callback identity

When the roster begins, the runtime captures its non-zero `wave_generation`.
That token is exposed in the runtime snapshot and must be retained alongside
any materialized EnemyObject/slot binding. Damage, explicit death, enemy-state,
and force-no-dead callbacks are accepted only when they carry the exact active
wave token.

This prevents a delayed callback from the completed Attack wave from mutating a
reused slot in a later wave. Resetting the runtime clears the active token while
`tutorial_enemy_roster` advances its internal generation, so the old token can
never become valid again. Slot indices alone are therefore not treated as
stable enemy identities.

## Completion edge

The runtime never marks the lesson complete from time or input alone. Enemy
health/death updates flow through `tutorial_enemy_roster`, and completion is
created only when `Roster::consume_all_defeated()` confirms that every planned
enemy was materialized and the recovered death predicate is true for the same
wave generation.

`consume_completion()` exposes that result as a one-shot edge. It deliberately
does not call `tutorial_gameplay_state::observe()` itself; the outer GL frame
integration owns publication of:

```text
Event::kTutorialEnemiesDefeated
```

After the one-shot edge is consumed, the wave remains latched `completed` while
the gameplay state is still `Attack`. This avoids touching the now-inactive
roster during the frame between event publication and the state-machine move to
`Block`. Once the gameplay step actually leaves `Attack`, all wave-local state
is reset.

## Remaining live integration

This module is linked into the Android native target but is not yet called from
`post_scene_overlay_jni.cpp`. The live hook remains blocked on the reviewed
providers that intentionally have no defaults:

- dynamic `PLAYERFACEDIR`;
- the three tutorial scene-limit values/order;
- reviewed enemy identity/base-HP mapping.

Once those providers are available, the frame boundary can resolve fresh
`tutorial_spawn_inputs`, tick this runtime with `game_clock::tick_count()`,
retain its `wave_generation` on each materialized enemy binding, forward real
combat damage/death state with that token, and publish the one-shot Attack
completion event.
