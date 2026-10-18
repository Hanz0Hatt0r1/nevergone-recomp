# Manual reconstruction: player level attack progression

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

`PlayerStatusInfo::levelUp(level)` starts with row index 1 and accumulates while
the index is less than `level`. For attack, it converts column 8 to float,
adds rows in order, then adds `PlayerStatusInfo +0x120`, then the resolved
`MRGetHeroEatAttribute(2, PlayerStatusInfo +0x154)` value. It stores the result
at `PlayerStatusInfo +0x110`; `getLevelAttack()` returns that field. Callers
obtain `level` from `MEPlayer::getPlayerLevelsNum()`, which reads
`PlayerData +0x60`.

The C++ contract now parses the ten-column row shape and retains column 8, then
reproduces that attack accumulation order. Level zero is valid: the native loop
starts at row index 1 and runs only while the row index is less than the level,
so level zero adds no CSV increments and still returns the two status bonuses.
Levels larger than the parsed table are rejected as a safety guard against
reading beyond available rows.

## Live level owner

`DataManager::getNoBasePlayerData()` (`ELF 0x0029e030`, RevEngInstr
`0x0039e030`, 496 bytes) recovers the saved-profile producer. With a non-null
profile at `DataManager +0x18`, it copies the profile C string at `+0x2dc` into
`PlayerData +0x3c`. It decrypts the unsigned profile field at `+0x114` and
stores the result in `PlayerData +0x60`. This is the runtime level field;
`DataManager::createData(uid)` calls `loadGameSaveData(uid)` first, then calls
`getNoBasePlayerData()` after its other profile-data loaders. The loader
(`ELF 0x0029d110`, RevEngInstr `0x0039d110`, 164 bytes) creates the save-data
object and reads it into `DataManager +0x18`. `loadGameSaveData` also reads the
saved-data block at `+0x144` and feeds its derived values into `PlayerStatusInfo`
accumulators for attack, damage absorption, health, strength restore, and
strength.

`MEPlayer::loadPlayerData()` (`ELF 0x00324eb2`, RevEngInstr `0x00334eb2`,
266 bytes) confirms how the runtime current level is consumed. It obtains
`PlayerData*` from `DataManager +0x44`, then reads `PlayerData +0x60` twice:
first it passes that value to `GameSceneUI::setMePlayerLevels()`, then—after
`GameScene::AllotPassLevelsExtraAward()`—it passes the same field to
`PlayerStatusInfo::levelUp()`. The status object is held at `DataManager +0x48`.

`MEPlayer::getPlayerLevelsNum()` (`ELF 0x0031c1f0`, RevEngInstr
`0x0032c1f0`) independently returns `DataManager +0x44 +0x60`. This confirms
that the level-table consumer and the ordinary player-level getter use the
same profile field. The native saved-profile-to-runtime mapping is recovered,
but first-save creation and server-side/profile-update values remain unknown.
The tutorial clone still has no profile-loading producer to supply this value.

## Scope

This restores the level-attack source and calculation. The reconstructed
tutorial currently has no player-profile level producer and its enemy contact
path only resolves collision and movement correction; those are still required
before this value can drive live enemy health. No level or weapon values are
invented here. Address evidence above comes from manual inspection of the local
original ARMv7 disassembly for the binary identified by the SHA-256 at the top;
the RevEngInstr address shown for each function uses the repository's `+0x10000`
address convention. These are manual disassembly labels, not REA output.
