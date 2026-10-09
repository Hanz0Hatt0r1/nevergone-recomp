# Fresh-role compatibility input

`FreshRoleCompatOverlay` is now a temporary project-owned Android input bridge for the **CharacterName** portion of the fresh-account role-creation path only. Native SingleSelectHero career runes, recovered transition state, rune touch, Confirm hit routing, and Confirm presentation own the career-selection surface.

The compatibility view is not a visual reconstruction of the original CharacterName controls. It remains only until the recovered `CharacterNameLayer` geometry and imported assets are connected to a native compositor/edit-box/IME implementation.

## Reused recovered semantics

The contiguous fresh-role path is:

```text
native SingleSelectHero career rune 1..5
  -> single_select_hero_state::select_career(career)

native SingleSelectHero Confirm
  -> single_select_hero_state::confirm_online()
  -> character_name_state::begin(career)

compatibility CharacterName text
  -> character_name_state::set_role_name(name)

CharacterName tag 1
  -> character_name_action_executor
  -> recovered <= 18 UTF-8-byte validation
  -> role_selection_state pending create request
  -> login_lua_session::dispatch_pending_role_create_request()
  -> g_UILogin.CreateCharacter(name, career)

CharacterName tag 2
  -> close semantic state

CharacterName tag 3
  -> recovered RandomName.csv executor
```

The career integer is never remapped. The recovered `SingleSelectHero::initUI()` creates five `menuOpenGC` items tagged `1..5`; `menuOpenGC()` stores that sender tag unchanged and `menuConfirm()` forwards it unchanged through `CharacterNameLayer::CretaUI` and `LUA_LOGIN::CreateTheRole` to `g_UILogin.CreateCharacter`.

The initial selection rule remains native state behavior: career 1 by default, except when the existing role is career 1, where the shipped path starts on career 2.

## Native career/Confirm ownership

The temporary compatibility career controls are retired from normal presentation. While `fresh_role_compat_state` reports career mode, `FreshRoleCompatOverlay` stays `GONE` and continues polling only so it can appear immediately when native Confirm opens CharacterName state.

The GLES path now owns:

- all five evidence-backed career rune centers and content-size hit rectangles;
- normal/pressed rune frames;
- recovered near/far career-change transition deadlines;
- the exact Confirm hit rectangle at the recovered `(836,70)` design center;
- `btn_a -> btn_b -> btn_d/e` Confirm presentation from the imported original atlas;
- `menuConfirm` dispatch into the existing semantic state.

The old Java career buttons remain instantiated only as a developer/compatibility implementation detail for existing JNI surfaces; they are not shown to the player.

## Remaining Android compatibility presentation

When `character_name_state` becomes active, the overlay exposes:

- an Android single-line text field / IME;
- Random -> recovered tag `3`;
- Confirm -> recovered tag `1`;
- Cancel -> recovered tag `2`.

Entering CharacterName mode invokes the recovered initial random-name action only while native state still marks it pending. A successful randomization therefore survives Activity recreation without generating a different second name. Failure to load imported `RandomName.csv` remains visible/retryable instead of fabricating a fallback name.

JNI carries semantic values and action tags only. It does not expose pending Lua requests or duplicate validation/network logic in Java.

## Relationship to recovered CharacterName geometry

The repository already contains renderer-independent `CharacterNameLayer::CretaUI` geometry for `Redbottom.png`, `RANDOMName.png`, the 30-pixel edit box, random control, Confirm/Cancel and modal blocker. This compatibility surface does not replace that work: it supplies an operable Android text/IME surface while the exact imported visual/control layer is still being wired into GLES.

## Tests and validation

The native SingleSelectHero path has focused host coverage for:

- career rune geometry and normal/pressed frame selection;
- rune DOWN/MOVE/UP/reset behavior;
- Confirm exact geometry and aspect-fit mapping;
- Confirm MOVE/cancel/multi-touch ownership;
- existing-career confirmation blocking;
- TexturePacker trim-aware Confirm quads.

`tools/fresh_role_compat_state_smoke.cpp` continues to verify CharacterName-mode precedence, career propagation, name synchronization, and semantic action routing. Main-branch full CI remains responsible for the real Gradle/NDK APK build and 16 KiB verification when GitHub runners are available.

## Removal condition / remaining gap

The compatibility overlay can now be removed once the CharacterName surface is native:

1. draw the recovered CharacterName panel and controls from user-imported original assets;
2. implement the recovered edit-box focus/modal geometry in the GLES/native route;
3. bridge native text focus and contents to Android IME without moving validation/network semantics into Java;
4. route Random / Confirm / Cancel through the existing `character_name_action_executor`.

The semantic states and action executor remain after removal; only the temporary Android text/IME presentation disappears.
