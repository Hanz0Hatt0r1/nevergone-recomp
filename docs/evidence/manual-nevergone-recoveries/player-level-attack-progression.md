# Manual reconstruction: player level stat progression

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

The imported game asset `files/assets/player/player_level.csv` has SHA-256
`1f95b897f7df8a04cd5d16d90170f020ec9d6927c90a2a5f8db9910bab9ffd9d`. It has
45 numeric rows with ten columns and no header. This is the table loaded by the
native player-level data path; `gamescene_ui/HeroLevel/AddPerLevel.csv` is a
separate five-column display table.

## Native data flow

`MEPlayer::initWithMEName()` calls
`MEPlayerLevelData::LoadPlayerLevelDataFile_csv()` with the literal
`player_level.csv` (ELF call site `0x00324e46`; string at file offset
`0x7714a8`). The loader calls `HCSVFile::openFile(path, false)`, reads ten
columns for each row, and stores them in parallel arrays at four-byte strides:

| CSV column | `MEPlayerLevelData` array offset | Native conversion |
| ---: | ---: | --- |
| 0 | `+0x14` | integer |
| 1 | `+0x1a4` | integer |
| 2 | `+0x334` | integer |
| 3 | `+0x4c4` | integer |
| 4 | `+0x654` | integer |
| 5 | `+0x7e4` | integer |
| 6 | `+0x974` | integer |
| 7 | `+0xb04` | float |
| 8 | `+0xc94` | integer |
| 9 | `+0xe24` | float |

`PlayerStatusInfo::levelUp()` starts at ELF `0x0031726c` (RevEngInstr
`0x0032726c`); `getALLStrengthRestore()` starts at ELF `0x003173b8`
(RevEngInstr `0x003273b8`).

`PlayerStatusInfo::levelUp(level)` starts with row index 1 and accumulates while
the index is less than `level`. It converts integer columns 5, 6, and 8 to
float, accumulates float column 7 directly, then adds the matching saved
addition fields:

| CSV column | Saved addition | Output field | Stat |
| ---: | ---: | ---: | --- |
| 5 | `PlayerStatusInfo +0x114` | `+0x104` | Health |
| 6 | `PlayerStatusInfo +0x118` | `+0x108` | Strength |
| 7 | `PlayerStatusInfo +0x11c` | `+0x10c` | Damage absorption |
| 8 | `PlayerStatusInfo +0x120` | `+0x110` | Attack |

Health then adds `MRGetHeroEatAttribute(1, +0x148)`, damage absorption adds
`MRGetHeroEatAttribute(5, +0x150)`, and attack adds
`MRGetHeroEatAttribute(2, +0x154)`. Strength has no food term in this method.
The saved strength-restore addition at `+0x124` is consumed separately by
`getALLStrengthRestore()`, which adds `MRGetHeroEatAttribute(4, +0x14c)`.
Callers obtain `level` from `MEPlayer::getPlayerLevelsNum()`, which reads
`PlayerData +0x60`.

The C++ contract parses all four level-stat columns, preserves row order, and
reproduces the native accumulator and addition order. Level zero is valid: the
native loop starts at row index 1 and runs only while the row index is less than
the level, so it adds no CSV increments while retaining saved and food bonuses.
Levels larger than the parsed table are rejected as a safety guard against
reading beyond available rows. `parse_player_level_attack_csv()` remains as a
compatibility name; `parse_player_level_status_csv()` exposes all four fields.

## Live level owner

`DataManager::getNoBasePlayerData()` (`ELF 0x0029e030`, RevEngInstr
`0x0039e030`, 496 bytes) recovers the saved-profile producer. With a non-null
profile at `DataManager +0x18`, it copies the profile C string at `+0x2dc` into
`PlayerData +0x3c`. It decrypts the unsigned profile field at `+0x114` and
stores the result in `PlayerData +0x60`. This is the runtime level field;
`DataManager::createData(uid)` (`ELF 0x0029e220`, RevEngInstr `0x0039e220`,
160 bytes) calls `loadGameSaveData(uid)` first, checks its other data loaders,
then calls `getNoBasePlayerData()`. After that succeeds, it loads armband data
and calls `MEPlayer::loadPlayerData()`. The save loader
(`ELF 0x0029d110`, RevEngInstr `0x0039d110`, 164 bytes) creates the save-data
object and reads it into `DataManager +0x18`. `loadGameSaveData` also reads the
saved-data block at `+0x144` and feeds its derived values into `PlayerStatusInfo`
accumulators for attack, damage absorption, health, strength restore, and
strength. The five serialized fields, offsets, and their shared management
gate are mapped in
[player-saved-stat-additions.md](player-saved-stat-additions.md).

### New save defaults

`GameSaveData::createGameSaveData(uid)` (`ELF 0x0033d168`, RevEngInstr
`0x0034d168`, 56 bytes) allocates the `0x324`-byte object, runs its constructor,
and calls `initGameSaveData(uid)`. The factory autoreleases the object only when
initialization succeeds; otherwise it destroys the object and returns null.
`DataManager::loadGameSaveData()` stores and retains this object before building
the path and calling `read(path, uid)`. It does not branch on `read()`'s return
value before reading the initialized `+0x144` status block and applying its
values to `PlayerStatusInfo`.

`GameSaveData::initGameSaveData(uid)` (`ELF 0x0033b6cc`, RevEngInstr
`0x0034b6cc`, 900 bytes) creates the save's arrays, a `PlayerStatusInfo` at
`GameSaveData +0x144`, and an `ArmbandItemData` at `+0x148`. It encrypts the
unsigned value `1` into `+0x114`, the field later copied to `PlayerData +0x60`
as the current player level. This establishes level 1 as the native default
object's starting level; later valid save data may replace it.

When `ManagementLayer +0x744` is zero, the initializer formats
`weapons/wa/wa_p%02d_001_001.wa` with `uid`, creates a `WeaponsItemData` using
type argument `1`, calls `buildFullWeaponsData()`, then calls
`addCurEquipArmorAndWeapon()`. That helper clones the weapon into the current
equipment array at `GameSaveData +0xc8` and updates its count at `+0xc4`. The
native initial equipment therefore depends on `uid`; no weapon attack value is
inferred here. The meaning of the `ManagementLayer +0x744` gate and the save
path's exact relationship to each career remain unresolved. The inspected
`buildFullWeaponsData()` body includes a conditional `lrand48()` call, so its
generated weapon values are not treated as deterministic defaults.

The initializer also builds a string in `GameSaveData +0x2dc`, the field copied
to `PlayerData +0x3c`. Its loop repeatedly calls `sprintf(field, "%s%d", field,
0)`. This establishes the string-writing operation but is not evidence that a
selected character name is stored there. The tutorial reconstruction's
selected-name-to-player path remains project-owned behavior pending a native
name producer.

`MEPlayer::loadPlayerData()` (`ELF 0x00324eb2`, RevEngInstr `0x00334eb2`,
266 bytes) confirms how the runtime current level is consumed. It obtains
`PlayerData*` from `DataManager +0x44`, then reads `PlayerData +0x60` twice:
first it passes that value to `GameSceneUI::setMePlayerLevels()`, then—after
`GameScene::AllotPassLevelsExtraAward()`—it passes the same field to
`PlayerStatusInfo::levelUp()`. The status object is held at `DataManager +0x48`.

`MEPlayer::getPlayerLevelsNum()` (`ELF 0x0031c1f0`, RevEngInstr
`0x0032c1f0`) independently returns `DataManager +0x44 +0x60`. This confirms
that the level-table consumer and the ordinary player-level getter use the
same profile field. The native saved-profile-to-runtime mapping and the
new-object default level are recovered. Server-side/profile-update values and
the tutorial clone's profile-loading producer remain unknown, so its runtime
still does not receive this level automatically.

## Scope

This restores the level-stat source and calculations in the behavior contract.
The reconstructed tutorial currently has no player-profile level producer and
its enemy contact path only resolves collision and movement correction; those
are still required before these values can drive live enemy health. No level or
weapon values are invented here. Address evidence above comes from manual
inspection of the local original ARMv7 disassembly for the binary identified
by the SHA-256 at the top;
the RevEngInstr address shown for each function uses the repository's `+0x10000`
address convention. These are manual disassembly labels, not REA output.
