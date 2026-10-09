# ChooseHero Play/Delete controls

This note records the clean-room reconstruction of the standalone ChooseHero fixed-button controls built by `ChooseHero::HeroInformation(int,bool)`.

## Type-1 MR fixed button resources

`HeroInformation()` calls `createMRFixedButton(MORUIUITYPE=1, ...)`. Resolving the PC-relative literals in that shipped helper gives the exact image paths:

- `Common/btn_standard_a.png` — normal;
- `Common/btn_standard_b.png` — selected/pressed;
- `Common/btn_standard_c.png` — disabled.

Those `Common` PNG files are referenced by the shipped native code but are not present in the baseline APK used for current clean-room analysis. `ChooseHeroActionControlLoader` therefore treats them as optional user-imported update/expansion assets. It never substitutes guessed original dimensions. If the three files are unavailable, the action controls remain `not-ready` while the already reconstructed hero items/profile/focus UI continues to work.

No proprietary button image bytes are stored in the repository.

## Play and Delete actions

The primary type-1 button receives sender tag `3`, which maps through the recovered `OnCreateback` contract to `start-selected-hero`. Its label is localized through key `PlayGame`.

In the local standalone branch (`ManagementLayer` mode flag zero), `HeroInformation()` creates a second type-1 button with sender tag `8`, which maps to `confirm-delete-selected-hero`. Its label is key `Prompt21`.

The reconstructed touch layer gives these buttons first refusal before the existing role-item router because the latter owns/consumes the rest of the choose-role screen. A completed DOWN/UP on the same fixed button dispatches the exact tag into `choose_hero_action_state`; this increment does not yet execute the downstream start/delete side effects.

## Recovered geometry

`ChooseHero::init()` stores `CCDirector::getVisibleSize()` at the `ChooseHero + 0x13c` `CCSize` field. `HeroInformation()` uses its width together with the type-1 button content size.

For the Play button:

```text
centerX = visibleWidth * 0.9
centerY = playButtonHeight + 5
```

For Delete:

```text
centerX = visibleWidth * 0.9 - playButtonWidth - 10
centerY = playButtonHeight + 5
```

The original obtains width/height from the created `CCMenuItemImage`; the recompilation therefore derives hit boxes from the imported `btn_standard_a.png` dimensions instead of hard-coding an observed or invented size.

The containing `CCMenu` is moved to the shared zero-position constant, so the button coordinates above remain layer-local design coordinates.

## Label presentation

`HeroInformation()` creates both fixed-button labels with:

- font family `Arial`;
- font size `24`;
- centered position at half the button content width/height.

`SetMRFixedButtonLableSize(type=1,label)` delegates to `SetMRLableSizeX(label,115)`. The shipped helper reads the label content width and:

- leaves scale at `1` when width is `<=115`;
- otherwise sets a uniform scale of `115 / labelWidth`.

The Android loader reproduces the existing Cocos2d-x Android `Paint`/`Canvas` text rasterization path and resolves `PlayGame` / `Prompt21` from the same imported `Login/ALL_Loin.csv` language column already used by the profile labels.

## Rendering and lifecycle

When assets are available, the renderer draws the fixed-button menu before the z=3 role-item pane, matching the recovered `HeroInformation()` hierarchy. Pressed controls switch from `_a` to `_b`; `_c` is staged for later disabled-state use.

The CPU asset store and GLES texture cache are generation-aware. Profile-asset reloads also trigger an independent action-control reload; failure to find the optional Common PNGs is deliberately ignored so baseline ChooseHero presentation is not degraded.

## Current boundary

The buttons now terminate at `choose_hero_action_state` requests:

- Play -> tag `3` / `start-selected-hero`;
- Delete -> tag `8` / `confirm-delete-selected-hero`.

The next execution increment should consume only the offline-safe portions of those requests. In particular, the local tag-3 branch can be reconstructed independently from the alternate legacy login-service branch, while tag 8 still needs its confirmation-frame/delete-save behavior reproduced before it may mutate user saves.
