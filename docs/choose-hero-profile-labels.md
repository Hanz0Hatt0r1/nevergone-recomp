# ChooseHero profile labels

This note documents the clean-room visual reconstruction of the existing-hero name, level and played-time labels shown inside the standalone ChooseHero item.

## Data sources

The compositor does not invent profile strings. It consumes the verified data layer:

- slot id `1` / `2` from the standalone save selection model;
- level from the recovered `SaveDataHero + 0x50` prefix field;
- played hours/minutes from `+0x64/+0x68`;
- visible hero name from localized `Hero1Name` / `Hero2Name` keys;
- level prefix from `GdUI08`;
- played-time prefix from `GameUSETime`.

Localization comes from the user-imported, decoded `Login/ALL_Loin.csv` resource.

## Shipped presentation contract

`ChooseHeroItem::createChooseHeroItem()` uses:

- font family `Arial`;
- size `20`;
- RGB `(96,96,96)`;
- `sel_hero_name_bg.png` from `Login/ChooseHero/`;
- name background center at local `(0.70 * itemWidth, 0.67 * itemHeight)`;
- name anchor `(0, 0.5)`, placed at the background left edge plus `20` pixels;
- level as a child of the name label at local `(nameLabel.contentWidth, 0)` with anchor `(0,0)`;
- `GameUSETime` anchor `(0,0.5)` with visible center at local X `0.80 * itemWidth`, Y `0.25 * itemHeight`.

The previously assumed `0.80W/0.25H` level position was incorrect; that formula belongs to the played-time label.

## Android text rasterization

The original Android Cocos2d-x path rasterizes `CCLabelTTF` text through Android `Paint`/`Canvas` and `Typeface.create(fontName, NORMAL)`. `ChooseHeroProfileLabelLoader` follows that behavior for `Arial` at 20 px and uploads the resulting ARGB bitmaps to the native renderer.

The Java bitmap is rendered white so anti-aliased alpha is preserved. The GLES compositor applies the recovered `(96,96,96)` tint at draw time.

This also preserves UTF-8 localized strings rather than reducing the label path to ASCII-only glyphs.

## Resource and lifecycle behavior

`sel_hero_name_bg.png` is present in the baseline APK and is loaded from the same app-private imported asset tree as the existing hero tiles. Profile label textures are regenerated when the runtime asset reload runs, and GLES texture names are generation-aware so EGL context loss or a later asset import does not leave stale texture objects.

Only slots with a valid recovered DMG prefix and all required localization keys receive profile label textures. Missing or malformed profile data therefore leaves the existing hero board/icon visible without fabricating values.

## Rendering order

The ChooseHero UI order is now:

1. storm / thunder / foreground cloud scene layers;
2. role board and hero/create icon;
3. existing-hero name background, name, level and played-time labels;
4. selected-item `focesItem()` highlight streaks.

The profile compositor is presentation-only and does not change role selection, touch routing, or scene-entry state.
