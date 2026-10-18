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
| `MEPlayer::setWeaponPower` | `0x00329adc` | 118 | 11 | 7 | 13 | 3 |
| `MEPlayer::addWeaponPower` | `0x00329ba8` | 113 | 11 | 5 | 4 | 2 |
| `MeActionsSystem::setWeaponPowerColor` | `0x0033bbe8` | 19 | 0 | 2 | 1 | 1 |

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

## Recovered hit ordinals and weapon-power meter

After the primary multipliers, `inputAtkValue()` applies these ordinal factors
to damage: ordinal 1 leaves it unchanged; ordinal 2 uses
`0.2 + supr * 0.0025`; ordinal 3 uses `0.15 + supr * 0.002`; ordinal 4 uses
`0.1 + supr * 0.0015`; and ordinals above 4 use `0.05 + supr * 0.001`.
The coefficients were read as IEEE-754 floats from the original ELF at
`0x003203a4`, `0x003203a8`, `0x00320654`, `0x00320658`, `0x0032065c`,
`0x00320660`, and `0x00320668`; the final base coefficient is `DAT_00330398`
at ELF `0x00320398`. A signed ordinal below 1 applies no factor and does not
request meter gain. Every ordinal at least 1 calls `addWeaponPower()` with the
already-resolved Meat attack category, including ordinal 1.

`MEPlayer::addWeaponPower()` ignores additions while player byte `+0x300` is
set. Otherwise, a positive incoming value gains an additional
`incoming * (EquipPropertyManager +0x8c / 100)`, then it is added to player
float `+0x30c` and floored at zero. Values below 100 update the meter UI. At
or above 100 the meter is written as exactly 100, `setWeaponPower(100)` is
called, and the native code requests shoot object `0x3eb` and plays its
activation sound. The comparison and reset values are both the original
ELF float 100 at `0x00319ba4`, `0x00319c68`, and `0x00319c6c`.

`setWeaponPower()` first checks that the action-system array for the current
player attack type exists. If it does not, the setter makes no changes. With
an array present it stores the requested meter, clears the active byte and
sets color index 0 below 100, or sets the active byte and color index
`equipped_weapon +0xe4 + 1` at 100 or above. It then updates the meter UI.
`MeActionsSystem::setWeaponPowerColor()` writes that integer converted to a
float into `+0x1c0` of each non-null frame sprite in its eight-slot array.
When threshold activation has no action-system array, `addWeaponPower()` has
already stored 100 and still issues the projectile and sound calls, while the
setter leaves the active byte and UI untouched. The new pure contracts expose
these transitions and side-effect requests; they are not yet connected to the
live enemy damage path.

## Recovered action-specific damage multipliers

After `DamageFactor()`, `inputAtkValue()` checks the player's attack type at
`+0x144`, action id at `+0x278`, and action context at `+0x274`. Each matching
branch multiplies damage by `1 + player_field`; branches run in the listed
order, so multiple matches compound. The new
`apply_action_specific_damage_multipliers()` accepts these values as ordered
arrays keyed by the enums in `player_damage_core_contract.h`.

For attack type 1, the field is selected by these action ids: `0x2e -> +0x458`;
context `0x24` or `0x1e` with action 9 -> `+0x45c`; 2–4 -> `+0x46c`; 5–6 ->
`+0x470`; 7–8 -> `+0x474`; `0x10 -> +0x478`; `0x27 -> +0x47c`; decimal 10 ->
`+0x480`; `0x0f -> +0x484`; `0x19`–`0x1a -> +0x488`; `0x13 -> +0x48c`;
`0x14 -> +0x490`; and `0x0c`–`0x0e -> +0x494`. For attack type 2: `0x2e ->
+0x4a4`; 2–5 -> `+0x4a8`; 6–7 -> `+0x4ac`; 8 -> `+0x4b0`; `0x19`–`0x1a ->
+0x4b4`; `0x35`–`0x36 -> +0x4b8`; `0x38 -> +0x4bc`; `0x13 -> +0x4c0`; decimal
10 -> `+0x4c4`; `0x0d -> +0x4c8`; `0x0e -> +0x4cc`; `0x0f -> +0x4d0`;
`0x10 -> +0x4d4` and again `+0x4dc`; `0x27 -> +0x4d8`; context `0x24` or
`0x1e` with action 8 -> `+0x4e0`.

Both attack types also apply one final multiplier for action ids `0x3a`–`0x3c`
or `0x3f`: type 1 reads `+0x49c`, type 2 reads `+0x4e8`. Other attack types
skip this action-specific block. Debug `printf`/`puts` calls were omitted from
the contract because they do not affect combat state.

## Scope

The reconstructed contracts cover the core expression, the listed property
bonuses, the four primary player multipliers, the weapon multiplier, hit
ordinals, the weapon-power meter, action-specific damage multipliers, and the
action-frame factor. The native function still has weapon/item effects, health
effects, super-energy side effects, and damage UI. The reconstruction renderer
still reports geometric contact and hit-window candidates only; it does not
yet send this resolved damage into the live enemy roster.
`player-damage-core-labels.jsonl` contains the `inputAtkValue` labels and the
new helper labels; the low-health predicate's gold label is reused from the
shared corpus. The local manual-restoration-v64 dataset contains 9,240 labelled
functions, 9,894 label rows, and 6,946 structural groups (7,420 train / 997
validation / 823 test). The generated corpus and dataset remain outside Git.
