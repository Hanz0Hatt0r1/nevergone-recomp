# Manual reconstruction: player damage core

> Address convention: RevEngInstr addresses are `0x10000` above the shipped
> ELF VMA. Subtract `0x10000` to inspect the ELF. Object offsets are unchanged.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Evidence

| Function | RevEngInstr entry | ELF entry | Body bytes | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `MEPlayer::inputAtkValue` | `0x0032ffa0` | `0x0031ffa0` | 3,792 | 15,851 | 151 | 170 | 240 | 10 |

The following callees are independently reconstructed in the damage path:

| Function | RevEngInstr entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `EquipPropertyManager::isAtk5BounsDamage` | `0x00326488` | 26 | 2 | 2 | 0 | 0 |
| `EquipPropertyManager::is_weapon_atk_up_on_attr` | `0x003265ae` | 20 | 2 | 2 | 3 | 0 |
| `EquipPropertyManager::is_weapon_atk_up_on_not_attr` | `0x003265de` | 20 | 2 | 2 | 3 | 0 |
| `MeActionsSystem::DamageFactor` | `0x0033bc06` | 13 | 0 | 0 | 3 | 0 |
| `EnemyObject::isLowHealthPercent` | `0x002c4f80` | 28 | 0 | 2 | 2 | 0 |

The corpus serializes this function's first 256 P-code operations and marks the
row truncated. The complete Ghidra 12.1.2 decompilation and ARM instructions
from the shipped library were inspected. The caller is
`BattleManager::playerHitEnemy()` at `0x002b7be0`; its first call to
`inputAtkValue` is at `0x002b7d28`.

Immediately before that call, the caller reads `EnemyObject +0x18c`, increments
it as an unsigned 16-bit value, stores the wrapped result, then sign-extends it
for the `short` argument. The call receives the current player, that hit
ordinal, the enemy, and a null final pointer. It occurs after point collision
and the player's hit-window check. The returned float is used by subsequent
combat effects and the super-energy update.

## Recovered core

`inputAtkValue` first resolves four weapon attack categories through the
`WeaponsItemData` getters, adds level attack, then applies property-manager,
equipment, weakness, and attack-state changes. Let `meat`, `supr`, and `base`
denote those already-adjusted category values. It transforms the enemy value
at `+0x2ec` with the equipped weapon fields `+0x8c`, `+0x90`, and `+0x94`,
clamping a negative result to zero. With `enemy_scale` from `EnemyObject +0x2f4`
and the player's attack type at `+0x144`, the exact core expression is:

```text
enemy_curve = max(0, weapon_90
                     + (enemy_2ec - weapon_8c)
                     * weapon_94
                     * (enemy_2ec - weapon_8c))
base_multiplier = 0.95 + (meat / enemy_scale) * 0.05
damage_core = (supr * 0.3 + (base + enemy_curve) * base_multiplier)
              * (attack_type == 2 ? 0.022 : 0.03)
```

The constants and left-to-right float additions match the decompiler and
instruction sequence. `player_damage_core_contract::resolve()` implements
this calculation and accepts the attack-category values after their native
producers have run.

## Recovered property bonuses

The pre-core base category starts with `getFinalAddBase_atk()` plus player
level attack. The property manager then selects `+0xa8` when
`is_weapon_atk_up_on_attr()` succeeds; otherwise it selects `+0x9c` when
`is_weapon_atk_up_on_not_attr()` succeeds. Both helper results require a
positive property value and a weapon at slot zero. The first helper additionally
requires weapon attribute `+0xa4 != 0x10`; the second requires `+0xa4 == 0x10`.

`isAtk5BounsDamage()` returns true when `lrand48() % 100 < 5`, in which case
`inputAtkValue()` adds EquipPropertyManager `+0x1c` to the base category. For a
positive property at `+0x94`, it adds
`min(VectorUnsignedToFloat(AppParameters +0x25c) * property_94, 100.0f)`. A
positive `+0x98` is added only when the player's attack type at `+0x144` equals
2. If `EnemyObject::isLowHealthPercent()` succeeds, a positive property at
`+0xa4` is added. That predicate computes
`trunc((EnemyObject +0x164 / EnemyObject +0x160) * 100)` and compares the
integer-converted percentage with 30. In AppParameters mode 9, the base also
receives `min(float(int(VectorUnsignedToFloat(AppParameters +0x230) *
EquipPropertyManager +0x88)), 200.0f)`.

`apply_property_attack_bonuses()` reproduces these ordered base additions.
The random source conversions are per-call decoded values in its terms;
`resolve_atk5_bonus_damage()`, the two weapon-attribute predicates, and
`resolve_enemy_is_low_health_percent()` reconstruct the branch conditions.

## Recovered primary multipliers

After the core expression, `inputAtkValue()` walks player floats `+0x420`,
`+0x424`, `+0x428`, and `+0x42c` in that order. It multiplies damage by each
value only when that value is below `1.0f`. If the byte at player `+0x300` is
set, the equipped weapon's `+0xe4` selects a further multiplier: `1.05f` for
zero, `1.15f` for one or any value other than two, and `1.25f` for two. The
function stores the adjusted Supe category at player `+0x438` before applying
that weapon multiplier.

Later, after the hit-ordinal and action-specific multipliers,
`MeActionsSystem::DamageFactor()` returns the float at
`*(MEActionsSystem +0x110) +0xcc + 4 * *(MEActionsSystem +0x1a4)`. The new
contract exposes the action-frame table lookup and applies the factor at that
same later stage. These exact stages are represented by
`apply_primary_damage_multipliers()`, `resolve_action_damage_factor()`, and
`apply_action_damage_factor()`.

## Scope

The recovered contracts now cover the core expression, the listed property
bonuses, the four primary player multipliers, the weapon multiplier, and the
action-frame factor. The native function still has hit-ordinal multipliers,
many action-specific buffs, weapon/item effects, health effects, super-energy
side effects, and damage UI. The reconstruction renderer still reports
geometric contact and hit-window candidates only; it does not yet send this
resolved damage into the live enemy roster. `player-damage-core-labels.jsonl`
contains the `inputAtkValue` labels and four new helper labels; the low-health
predicate's gold label is reused from the shared corpus. Adding the four new
functions to manual-restoration-v62 and rebuilding as v63 yields 9,237 labelled functions,
9,889 label rows, and 6,943 structural groups; the split is 7,418 train / 996
validation / 823 test. The generated corpus and dataset remain outside Git.
