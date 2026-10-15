# EnemyObject health and death contract

This note records the minimal clean-room health/death behavior required by the
first tutorial combat slice. It is derived from the original ARMv7
`EnemyObject` methods and contains no original data tables or binary bytes.

## Retained fields

The original object layout uses two adjacent floating-point fields:

```text
+0x160  maximum health
+0x164  current health
```

`EnemyObject::loadEnemyObjectFile(GameLevelsEnemyData*, ...)` reads the enemy
health integer from the imported data record and writes the same converted float
into both fields. The project-owned state therefore initializes `max_health`
and `current_health` from one positive base-health value.

`EnemyObject::enemyHealthRatio()` computes exactly:

```text
current_health / max_health * 100.0
```

The multiplier is the original `100.0f` literal.

## Ordinary hit mutation

At the beginning of `EnemyObject::beaten(float, float, bool*, int)`, after a few
special state exclusions, the first float argument is subtracted directly from
current health:

```text
current_health -= damage
```

The ordinary mutation does not clamp to zero. Subsequent original code has
special boss/chapter branches that may impose minimum-health thresholds; those
special cases are intentionally outside this first tutorial contract.

`EnemyObject::addHealth(float)` likewise performs a direct addition without a
clamp.

`EnemyObject::setEnemyDead()` writes zero into the current-health field before
calling the normal death predicate and selecting its death state.

## `isDead()`

The recovered predicate is compact and exact for the retained fields:

```text
if current_health > 0:
    false
else if enemy_state == 0x0f:
    false
else if enemy_state == 0x2c:
    false
else:
    !force_action_state_no_dead
```

The suppression byte is the field set by
`EnemyObject::forceCurActionStateNoDead()` at original object offset `+0x19d`.
Setting that byte prevents the ordinary `isDead()` result even when health is
non-positive.

The two excluded `ENEMYSTATE` values are retained numerically until their enum
names are independently recovered; assigning guessed semantic names would add
unsupported information.

## Relation to tutorial live-enemy counting

The separately recovered `BattleManager::enemyNullCount()` iterates the live
enemy array and increments its result for each object whose `isDead()` returns
false. A project-owned enemy roster can therefore reproduce the tutorial gates
without inventing a different death rule:

```text
alive_count = count(entries where !enemy_health_state::is_dead(entry.health))
```

The source of each enemy's initial base-health value remains a separate import
boundary. This module accepts that value as input; it does not hard-code HP for
tutorial enemy IDs 3 or 0x15.

## Scope boundary

This contract does not yet reconstruct hitboxes, attack power, armor, AI,
knockback, special boss health floors, rewards, death animation, or object
removal. It establishes only the health mutation and liveness predicate needed
for the next live tutorial roster/combat slice.
