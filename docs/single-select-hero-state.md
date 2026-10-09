# SingleSelectHero online career-selection contract

This note records clean-room behavior recovered from the shipped ARMv7 `SingleSelectHero::init(bool)`, `SingleSelectHero::initUI()`, `SingleSelectHero::menuOpenGC(CCObject*)`, `SingleSelectHero::OpenTheDoor(bool,int)`, and `SingleSelectHero::menuConfirm(CCObject*)` paths. Original disassembly and proprietary asset bytes are not stored in the repository.

## Supported careers

`initUI()` constructs five `menuOpenGC` items, assigns sender tags `1` through `5` unchanged and formats `xrfuwen%02d.png` / `xrfuwenfaguang%02d.png`. The recovered career domain is therefore `1..5`, superseding the earlier partial two-career model.

The item centers use `x = visibleWidth * 0.5` and `y = visibleHeight - 55 - 90 * career`, producing `(568,495)`, `(568,405)`, `(568,315)`, `(568,225)`, `(568,135)` on the 1136x640 design surface.

## Existing-career and initial selection

A valid existing career `1..5` must be preserved because `menuConfirm()` compares it directly with the selected career. `initUI()` still starts with career `1`, except when the existing career is `1`, in which case the initial candidate is `2`.

## Selection and transition gate

`menuOpenGC()` reads the sender tag and stores it unchanged into the selected-career fields. `OpenTheDoor(false, ...)` locks career changes while the transition is active; `OpenTheDoor(true, ...)` restores them. Selecting the already displayed career is a handled no-op. The existing-career equality check belongs to `menuConfirm()`, not `menuOpenGC()`.

## Online confirm path

`menuConfirm()` blocks when the selected career equals the existing role career. Otherwise it creates `CharacterNameLayer` and calls `CretaUI(selectedCareer)`. The same integer is preserved through `LUA_LOGIN::CreateTheRole(name, career)` and `g_UILogin.CreateCharacter(name, career)`; the recovered `createRoleParameter` is therefore the career value itself.

`single_select_hero_state` now models valid careers `1..5`, preserves existing careers across that range, keeps the exact initial-selection rule and transition gate, and forwards the selected career unchanged to `character_name_state`.

## Current boundary

The compositor now has the reconstructed background, HeroTable and five career-rune frames. Remaining interaction work is to derive hit rectangles from imported rune content sizes, represent the transition unlock point, route career DOWN/UP state into `select_career()`, and recover the confirm-control geometry before calling `confirm_online()`.
