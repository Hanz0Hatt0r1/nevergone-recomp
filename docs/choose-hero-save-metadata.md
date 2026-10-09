# ChooseHero standalone save metadata

This note records the clean-room prefix of the shipped `DMG_01.sData` / `DMG_02.sData` format needed by the standalone ChooseHero pane. No original save bytes are stored in the repository.

## Wire format

`GameSaveData::write()` serializes the save as an ASCII token stream separated by literal spaces. The paired `readDecodeSaveDataL` / `readDecodeSaveDataS` helpers advance to the next space after every integer or string token.

The recovered prefix is:

1. save magic `447389477` (`0x1AAA9F25`);
2. current device UUID string;
3. save version;
4. device-history count;
5. that many historical UUID string tokens;
6. thirteen integer fields copied into the temporary `SaveDataHero` used by `LoadStandaloneHeroDataList()`.

The current shipped writer emits version `10003` (`0x2713`). The loader rejects the old `<=10001` path before constructing a standalone hero. The writer limits the device-history list to five entries, so the project-owned parser rejects larger counts instead of indexing an unbounded prefix.

The thirteen hero integers are assigned in this order:

```text
SaveDataHero +0x40
SaveDataHero +0x44
SaveDataHero +0xD4
SaveDataHero +0x48
SaveDataHero +0x4C
SaveDataHero +0x50   <- level
SaveDataHero +0x54
SaveDataHero +0x58
SaveDataHero +0x5C
SaveDataHero +0x60
SaveDataHero +0x64   <- played hours used by GameUSETime
SaveDataHero +0x68   <- played minutes used by GameUSETime
SaveDataHero +0x6C
```

`SaveDataHero` itself is `0xE4` bytes in this build. The standalone loader writes the file slot id separately to `SaveDataHero +0x3C`; it is not another serialized token in this prefix.

`standalone_hero_save_metadata` stops after these thirteen values. Later save sections are deliberately not parsed until their schemas are proven independently.

## Hero display name is localization, not save data

A prior working assumption that `SaveDataHero +0x1C` held the hero name was incorrect. The shipped loader constructs that field as the localized `GameUSETime` string plus an `%02d:%02d` time value.

For the visible hero name, `LoadStandaloneHeroDataList()` instead formats the key:

```text
Hero%dName
```

with the standalone slot id and resolves it through `ManagementLayer::GetPlistString`. Therefore slot 1 uses `Hero1Name` and slot 2 uses `Hero2Name`; the player-facing name is not stored in the DMG file.

The baseline APK contains those rows in `Login/ALL_Loin.csv`. `OriginalApkImporter` already decodes `.csv` resources into `files/assets`, so the reconstructed runtime can read that CSV directly after APK import.

Confirmed rows include:

```text
Hero1Name -> English DAWN
Hero2Name -> English HIGGS
GdUI08    -> English Lv.
GameUSETime -> English Time 
```

No proprietary CSV content is checked into the project; these examples document the semantic keys/values recovered during analysis.

## Localization columns

`ManagementLayer::GetPlistString()` indexes the CSV by the language-column value initialized from `SystemLanguage::ReturnSystemLanguage()`. Confirmed zero-based columns are:

- 2: simplified Chinese;
- 3: traditional Chinese;
- 4: English/default;
- 6: French;
- 7: German;
- 8: Japanese.

`login_localization_csv` takes an explicit column index. Platform-language selection remains a separate integration boundary, avoiding an invented mapping for any unresolved system-language cases.

## ChooseHero label evidence

`ChooseHeroItem::createChooseHeroItem()` uses the localized name and level with these shipped presentation constants:

- font `Arial`;
- size `20`;
- RGB `(96,96,96)`;
- `sel_hero_name_bg.png` at `(0.70 * itemWidth, 0.67 * itemHeight)`;
- name X = `backgroundX - backgroundWidth/2 + 20`, name Y = `backgroundY`;
- level X = `0.80 * itemWidth - labelWidth/2`;
- level Y = `0.25 * itemHeight`;
- level prefix comes from localized key `GdUI08`.

This PR establishes the data source only. Text rasterization and `sel_hero_name_bg.png` rendering are intentionally left for the next visual increment so font behavior can be validated independently.

## Validation boundary

The host regression checks:

- exact magic and shipped version acceptance boundary;
- device-history count `0..5`;
- truncated prefixes;
- slot ids restricted to `1` and `2`;
- level and play-time field positions;
- terminal NUL tolerance outside the token stream;
- UTF-8 CSV lookup;
- quoted CSV fields containing commas;
- missing keys/columns.

Runtime diagnostics report only metadata from saves that pass this parser. The existing route-selection probe is not changed by this increment, so compatibility behavior for previously detected files remains stable until the metadata reader is deliberately connected to scene routing.
