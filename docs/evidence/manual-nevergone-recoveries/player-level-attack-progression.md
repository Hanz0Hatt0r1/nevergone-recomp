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
reproduces that attack accumulation order. It rejects level zero and levels
larger than the parsed table so callers cannot read past the available data.

## Scope

This restores the level-attack source and calculation. The reconstructed
tutorial currently has no player-profile level producer and its enemy contact
path only resolves collision and movement correction; those are still required
before this value can drive live enemy health. No level or weapon values are
invented here.
