# Manual reconstruction: enemy beaten damage and death handoff

> RevEngInstr executable addresses use the recovered image base. Object-field
> offsets are unchanged.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Native path

`EnemyObject::beaten(float, float, bool*, int)` begins its ordinary path by
subtracting the supplied damage from `EnemyObject +0x164` and adding the
supplied combo value to `+0x2fc`. It then applies event-specific health clamps,
reports the health change, advances configured physical state, and evaluates
`EnemyObject::isDead()`.

For the ordinary dead branch, the native code selects state `0x28` for enemy
id 5 and state `0x15` for other enemy ids, then calls `setEnemyState(state,
1, 1, -1)`. Special event health clamps and physical-state reactions are
separate branches and are not folded into the retained generic damage helper.

`EnemyObject::tickEnemy()` pauses its action system when `isDead()` is true,
then continues the native step path. In the current tutorial renderer, a dead
enemy therefore keeps its selected state action at its first frame and no
longer contributes a collision body.

## Reconstruction connection

`enemy_health_state::apply_beaten_damage_and_death_state()` applies the
ordinary health subtraction and terminal state handoff. The tutorial roster
uses it for its existing generation-checked damage entry point. After damage,
`tutorial_attack_live_gate::Gate::apply_damage()` synchronizes the actor
binding, so the render queue observes the new enemy state in the same wave.

The helper intentionally does not implement event-specific health clamps,
health-data callbacks, enemy-physical damage scaling, combo bookkeeping, or hit
effects. The direct player-hit detector still reports contact candidates; the
full `MEPlayer::inputAtkValue()` result remains disconnected from this damage
entry point.

Gold subsystem and function-role labels for `EnemyObject::beaten()` are
recorded in `enemy-beaten-death-handoff-labels.jsonl`.
