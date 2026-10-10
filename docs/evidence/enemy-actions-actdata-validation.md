# EnemyActions action-data validation against user-owned APK

Target APK package: `com.hippiegame.nevergone`, original game version 1.0.9. Native library SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note records structural metadata only. No proprietary action-data payload is committed.

## Native path construction

Thumb disassembly of `EnemyActionsSystem::initWithFile(char const*, ...)` at ELF offset `0x2adcb0` shows that the action-data filename is supplied by the caller. The method does not append a `.wbg` extension.

For object-type selector values observed in this routine:

- `1` formats `npc%02d/adata/%s`;
- `2` formats `pet%02d/adata/%s`;
- `3` formats `l%02d/adata/%s`.

Selector `0` goes through `ActionDataManager::addActionDataWithID(..., char const*)` using the caller-supplied filename. Therefore `loadWBGFile` is a method name, not evidence for an on-disk `.wbg` suffix.

## Genuine files present in the user-owned APK

Three `*.actData` files are present under `assets/gamescene_ui/SingleLogin_UI/actions/`. Each was checked against the reconstructed Sections A-G topology. The table contains only filename, size, SHA-256, and structural counts.

| File | Bytes | SHA-256 | Header | Primary | B records | C groups/records | D groups/records | E records | F tuples | G entries | Trailing |
| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `Se04_bron01_a.actData` | 11172 | `ae7354984764afd81b9c93e9301f02d75ad3379dd67671124f81580e8ba81d24` | 104 | 37 | 37 | 0 / 0 | 6 / 37 | 0 | 37 | 37 | 0 |
| `Se04_bron01_b.actData` | 6372 | `7c05dd0cc9891fd8920c1bfcb4861527ed32cb1d7c1286da649db2c1fea095df` | 104 | 21 | 21 | 0 / 0 | 6 / 21 | 0 | 21 | 21 | 0 |
| `Se04_bron01_c.actData` | 9672 | `a829738a4c16b8e28ce5054d771f75bc73468d390c24f82420ecd8f6f0712364` | 104 | 32 | 32 | 0 / 0 | 6 / 32 | 0 | 32 | 32 | 0 |

For all three files, `bytes_consumed == input_bytes`; the recovered A-G boundary reaches exact EOF.

This is the first validation of the composed parser against genuine user-owned Never Gone action data rather than a synthetic fixture.

## Scope

These results prove that the recovered structural parser matches these three original `.actData` files closely enough to consume their full serialized topology with internally consistent section counts. They do not prove gameplay meanings for individual fields, malformed-file behavior, or that every action-data variant in the external OBB has the same section population.

The batch scanner therefore recognizes `.actData` case-insensitively in addition to `.wbg`, while retaining `.wbg` for existing research fixtures and any user-owned files with that suffix.
