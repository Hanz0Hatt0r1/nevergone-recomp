# Manual reconstruction: saved player stat additions

> Address convention: executable addresses from RevEngInstr use an image base
> `0x10000` above the shipped ELF VMA. Subtract `0x10000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.
> Object-field offsets are unchanged.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Five saved additions

`PlayerStatusInfo` stores these five float values at `+0x114` through `+0x124`:

| Value | Field | Getter | Setter | Native adder |
| --- | ---: | --- | --- | --- |
| Health | `+0x114` | `getAddHealthValue()` | `setAddHealthValue()` | `add_AddHealthValue()` |
| Strength | `+0x118` | `getAddStrengthValue()` | `setAddStrengthValue()` | `add_AddStrengthValue()` |
| Damage absorption | `+0x11c` | `getAddDamageAbsorbValue()` | `setAddDamageAbsorbValue()` | `add_AddDamageAbsorbValue()` |
| Attack | `+0x120` | `getAddAttackValue()` | `setAddAttackValue()` | `add_AddAttackValue()` |
| Strength restore | `+0x124` | `getAddStrengthRestore()` | `setAddStrengthRestore()` | `add_AddStrengthRestore()` |

Function entries below show ELF / RevEngInstr addresses:

| Function | ELF | RevEngInstr |
| --- | ---: | ---: |
| `resetPlayerStatusInfoAddData()` | `0x0031723c` | `0x0032723c` |
| Health getter / setter / adder | `0x0031739a` / `0x0031737c` / `0x003174f0` | `0x0032739a` / `0x0032737c` / `0x003274f0` |
| Strength getter / setter / adder | `0x003173a0` / `0x00317382` / `0x00317518` | `0x003273a0` / `0x00327382` / `0x00327518` |
| Damage-absorption getter / setter / adder | `0x003173a6` / `0x00317388` / `0x00317540` | `0x003273a6` / `0x00327388` / `0x00327540` |
| Attack getter / setter / adder | `0x003173ac` / `0x0031738e` / `0x00317568` | `0x003273ac` / `0x0032738e` / `0x00327568` |
| Strength-restore getter / setter / adder | `0x003173b2` / `0x00317394` / `0x00317590` | `0x003273b2` / `0x00327394` / `0x00327590` |
| `GameSaveData::write()` | `0x0033be38` | `0x0034be38` |
| `GameSaveData::read()` | `0x0033e4a4` | `0x0034e4a4` |
| `DataManager::loadGameSaveData()` | `0x0029d110` | `0x0039d110` |

The getters are one-load functions and the setters are one-store functions at
the corresponding offsets. Each `add_Add*Value()` calls
`ManagementLayer::sharedManagementLayer()`, reads its byte at `+0x744`, and
adds the argument to its matching field only when that byte is zero. A nonzero
gate leaves the field unchanged. `resetPlayerStatusInfoAddData()` clears all
five fields.

`GameSaveData::write()` serializes these values in this order: health, strength,
strength restore, attack, damage absorption. The corresponding parser reads
the five floats in that same order. It applies the decoded values through
setters for health, strength, damage absorption, attack, then strength restore;
the third, fourth and fifth decoded values are held until their matching setter
calls. The parser is reached through `GameSaveData::read()` after its save-file
check succeeds.

`DataManager::loadGameSaveData()` reads the status object at
`GameSaveData +0x144` and transfers its values to the live status object in
this order: attack, damage absorption, health, strength restore, strength.
Those calls use the same gated adders as the property reward and skill-point
paths, so loaded values accumulate into the initialized live fields rather
than replacing them.

`PlayerStatusInfo::levelUp()` folds the health, strength, damage absorption,
and attack additions into the level values at `+0x104`, `+0x108`, `+0x10c`,
and `+0x110`. `getALLStrengthRestore()` adds `+0x124` to the resolved food
strength-restore value. In particular, `+0x120` is the saved attack addition
already passed to the `levelUp()` contract documented in
[player-level-attack-progression.md](player-level-attack-progression.md).

The C++ contract exposes the five fields, their shared gated accumulation,
native reset, and how level refresh consumes them. It records these recovered
writes as a bounded behavior contract; the prototype's separate save runtime
does not yet load original `GameSaveData` files into this state.

Function-role and subsystem gold labels are recorded in
`player-saved-stat-additions-labels.jsonl`.
