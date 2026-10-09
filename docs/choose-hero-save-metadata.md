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

`standalone_hero_save_metadata` stops after these thirteen values. Later save sections are deliberately not parsed until their schemas are proven independently. Runtime prefix reads are bounded to 64 KiB.

## Hero display name is localization, not save data

A prior working assumption that `SaveDataHero +0x1C` held the hero name was incorrect. The shipped loader constructs that field as the localized `GameUSETime` string plus an `%02d:%02d` time value.

For the visible hero name, `LoadStandaloneHeroDataList()` instead formats the key:

```text
Hero%dName
```

with the standalone slot id and resolves it through `ManagementLayer::GetPlistString`. Therefore slot 1 uses `Hero1Name` and slot 2 uses `Hero2Name`; the player-facing name is not stored in the DMG file.

The baseline APK contains those rows in `Login/ALL_Loin.csv`. `OriginalApkImporter` already decodes `.csv` resources into `files/assets`, so the reconstructed runtime can read that CSV directly after APK import.

Confirmed semantic examples include English `Hero1Name = DAWN`, `Hero2Name = HIGGS`, `GdUI08 = Lv.` and the `GameUSETime` prefix. Original CSV bytes are not checked into the repository.

## Localization columns

`ManagementLayer::GetPlistString()` indexes the CSV by the language-column value initialized from `SystemLanguage::ReturnSystemLanguage()`. Confirmed zero-based columns are:

- 2: simplified Chinese;
- 3: traditional Chinese;
- 4: English/default;
- 6: French;
- 7: German;
- 8: Japanese.

The shipped language switch falls back to column 4 for English and for system-language enums corresponding to Italian, Spanish, Russian, Korean, unknown and out-of-range values. Chinese selects column 2 or 3 according to the traditional-Chinese probe (`zh-Hant` in the Android bridge). The reconstructed Android loader mirrors that behavior, using Hant/TW/HK/MO locale information for the traditional branch and English for unsupported languages.

## ChooseHero label evidence

`ChooseHeroItem::createChooseHeroItem()` uses these shipped presentation constants:

- font `Arial`;
- size `20`;
- RGB `(96,96,96)`;
- `sel_hero_name_bg.png` at `(0.70 * itemWidth, 0.67 * itemHeight)`;
- name anchor `(0, 0.5)`;
- name X = `backgroundX - backgroundWidth/2 + 20`, name Y = `backgroundY`;
- level prefix comes from localized key `GdUI08`;
- level label is a child of the name label at local `(nameLabel.contentWidth, 0)` with anchor `(0,0)`;
- GameUSETime anchor is `(0,0.5)`;
- GameUSETime uses `x = 0.80 * itemWidth - labelWidth/2`, `y = 0.25 * itemHeight`; because the X anchor is zero, its visible center is at `0.80 * itemWidth`.

The previously documented `0.80W / 0.25H` placement for the level label was incorrect; focused ARMv7 inspection shows that formula belongs to GameUSETime.

The baseline APK also contains `Login/ChooseHero/sel_hero_name_bg.png` (122x29 in the analyzed build), so the profile background can be loaded directly from user-imported APK assets.

## Android text rasterization

The Android Cocos2d-x 2.x path used by the shipped game creates TTF label textures with Android `Paint`: anti-aliasing enabled, `Typeface.create(fontName, NORMAL)`, `measureText` rounded up for unconstrained width, font-metric height rounded up, and the baseline at `-fontMetrics.top` for an unconstrained label.

`ChooseHeroProfileLabelLoader` follows that behavior with `Arial` size 20, generates white ARGB label textures, and the GLES profile compositor applies the shipped `(96,96,96)` node color as an RGB tint. This preserves UTF-8 localized text without introducing a project-specific bitmap font.

## Validation boundary

Regression coverage checks:

- exact magic and shipped version acceptance boundary;
- device-history count `0..5`;
- truncated prefixes and integer overflow rejection;
- slot ids restricted to `1` and `2`;
- level and play-time field positions;
- terminal NUL tolerance outside the token stream;
- UTF-8 CSV lookup and quoted CSV fields containing commas;
- localized name/level/GameUSETime composition for multiple language columns.

Runtime diagnostics report only metadata from saves that pass this parser. The existing route-selection probe remains separate, while the profile compositor consumes verified metadata only when a matching valid slot is available.
