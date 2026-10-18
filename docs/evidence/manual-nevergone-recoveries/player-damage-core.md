# Manual reconstruction: player damage core

> Address convention: RevEngInstr addresses are `0x10000` above the shipped
> ELF VMA. Subtract `0x10000` to inspect the ELF. Object offsets are unchanged.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Evidence

| Function | RevEngInstr entry | ELF entry | Body bytes | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `MEPlayer::inputAtkValue` | `0x0032ffa0` | `0x0031ffa0` | 3,792 | 15,851 | 151 | 170 | 240 | 10 |

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

## Scope

This is the core calculation, not the full `inputAtkValue` routine. The native
function continues with four player multipliers, an equipment multiplier,
`MeActionsSystem::DamageFactor()`, action-specific buffs, item effects,
health effects, and damage UI. The reconstruction renderer still reports
geometric contact and hit-window candidates only; no damage is applied until
those remaining inputs and effects have runtime producers. The recovered
function-role and combat-subsystem labels are in
`player-damage-core-labels.jsonl`.
