# Fresh-role compatibility input

This increment adds a temporary project-owned Android input bridge for the fresh-account role-creation path. It exists to keep the contiguous P4/P5 path operable while the shipped `SingleSelectHero` career/confirm touch routing and door/carousel timing are still being reconstructed, and while the recovered `CharacterNameLayer` geometry has not yet been connected to a native compositor/edit-box implementation.

It is **not** presented as a visual reconstruction of the original controls. No guessed original hit boxes, artwork, or animation timing are introduced.

## Reused recovered semantics

The bridge delegates to the existing evidence-backed state machines:

```text
SingleSelectHero career 1..5
  -> single_select_hero_state::select_career(career)
  -> single_select_hero_state::confirm_online()
  -> character_name_state::begin(career)

CharacterName text
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

The initial selection rule remains native state behavior: career 1 by default, except when the existing role is career 1, where the shipped path starts on career 2. The Android controls do not duplicate that policy.

## Compatibility-only transition rule

The shipped `menuOpenGC()` is locked while `OpenTheDoor` / Carousel transition work is active. The native visual executor for that transition is not complete yet.

`fresh_role_compat_state::select_career()` therefore performs one explicit compatibility-only shortcut:

1. if the recovered state is waiting for its initial transition, call `complete_transition()`;
2. dispatch the exact career through `select_career()`;
3. if the changed career starts another recovered transition, immediately complete it.

This shortcut exists only behind the temporary Android controls. It does not alter `single_select_hero_state` itself, so the future native animation/touch implementation can retain the recovered timing contract.

`menuConfirm()` is not given an invented transition gate: the recovered handler itself does not test the `menuOpenGC` interaction byte.

## Android presentation

`FreshRoleCompatOverlay` lives in the existing development/control pane below the GLES surface and is `GONE` outside the relevant semantic states.

Career-selection mode exposes all five recovered sender values:

- careers 1, 2, 3, 4, 5;
- Confirm career.

Character-name mode exposes:

- an Android single-line text field / IME;
- Random -> recovered tag `3`;
- Confirm -> recovered tag `1`;
- Cancel -> recovered tag `2`.

Entering CharacterName mode invokes the recovered initial random-name action only while the native state still marks it pending. A successful randomization therefore survives Activity recreation without generating a different second name. Failure to load imported `RandomName.csv` remains visible/retryable instead of fabricating a fallback name.

JNI only carries semantic values and action tags. It does not expose pending Lua requests or duplicate validation/network logic in Java.

## Relationship to recovered CharacterName geometry

PR #162 recovered the exact renderer-independent `CharacterNameLayer::CretaUI` layout formulas for `Redbottom.png`, `RANDOMName.png`, the 30-pixel edit box, random control, Confirm/Cancel and modal blocker. This compatibility surface does not replace or contradict that work: it simply supplies an operable Android edit/IME surface until those formulas and imported assets are wired into a native CharacterName compositor/input router.

## Tests

`tools/fresh_role_compat_state_smoke.cpp` verifies:

- hidden / career / character-name mode precedence;
- the explicit compatibility transition collapse;
- rejection outside the recovered `1..5` career range;
- unchanged career `5` propagation into CharacterName state;
- name synchronization;
- the recovered existing-career confirmation block for career `5`.

The Android native library compiles the JNI bridge, while normal main-branch full CI remains responsible for the real Gradle/NDK APK build and 16 KiB verification.

## Removal condition / remaining gap

Remove this compatibility surface once all of the following are connected in the GLES path:

1. evidence-backed SingleSelectHero career/confirm hit boxes using the imported frame content sizes and recovered menu centers;
2. recovered OpenTheDoor / Carousel transition timing and unlock point;
3. a native CharacterName compositor using the already recovered `character_name_layout` geometry and imported assets;
4. native CharacterName edit-box focus/IME plumbing and confirm/random/cancel routing using that recovered layout.

The semantic state and `character_name_action_executor` should remain; only this temporary Android presentation and transition shortcut should disappear.
