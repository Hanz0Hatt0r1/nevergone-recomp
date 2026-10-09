# SingleSelectHero online career-selection contract

This note records clean-room behavior recovered from the shipped ARMv7 `SingleSelectHero::init(bool)`, `SingleSelectHero::initUI()`, `SingleSelectHero::menuOpenGC(CCObject*)`, and `SingleSelectHero::menuConfirm(CCObject*)` paths. Original disassembly and proprietary asset bytes are not stored in the repository.

## Supported careers

The recovered selector presentation is built around exactly two career values:

- career `1`;
- career `2`.

`Carousel(int)` has explicit presentation branches for values up to `2`, and the initial-selection logic chooses only `1` or `2`.

## Existing-career input and default selection

During `init(bool)`, SingleSelectHero reads the first existing role record from ManagementLayer state when one is present and stores that role's career in its existing-career field.

`initUI()` then performs this exact selection setup:

```text
selectedCandidate = 1
if existingCareer == 1:
    selectedCandidate = 2
selectedCareer = selectedCandidate
Carousel(selectedCandidate)
```

Therefore:

- no existing supported career -> default career `1`;
- existing career `1` -> default career `2`;
- existing career `2` -> default career `1`.

This matches the two-career create-role flow without inventing an additional class table.

## `menuOpenGC` selection semantics

The class buttons carry their career directly as the sender tag. After the shipped animation/input gates allow interaction, `menuOpenGC()`:

1. reads the sender tag;
2. ignores the click when the same career is already selected;
3. runs the door/carousel transition for a changed career;
4. writes the sender tag unchanged into both selected-career fields.

The presentation-independent recompilation therefore accepts only career tags `1` and `2` and stores them without conversion. Animation gating remains the compositor/input layer's responsibility.

## Online confirm path

`menuConfirm()` compares the current candidate career against the existing role's career. If they are equal, the shipped function returns without entering role creation.

For the online ManagementLayer branch, a valid different career follows this path:

```text
selectedCareer
  -> CharacterNameLayer::create()
  -> CharacterNameLayer::CretaUI(selectedCareer)
```

The same integer later reaches `LUA_LOGIN::CreateTheRole(name, career)` and `g_UILogin.CreateCharacter(name, career)` through the already reconstructed CharacterNameLayer contract.

`single_select_hero_state::confirm_online()` therefore opens `character_name_state` with the selected career unchanged and records blocked confirms separately.

## Current implementation boundary

`single_select_hero_state` now owns the non-visual semantics:

- active scene generation;
- existing career;
- exact default selection rule;
- valid careers `1` and `2`;
- sender-tag selection behavior;
- duplicate/existing-career confirm blocking;
- online confirm bridge into `character_name_state::begin(career)`;
- diagnostics counters.

The current `single_select_hero_compositor` still renders only the previously reconstructed background/opening presentation. Rendering the two career choices, recovering their hit boxes and wiring touch into this state are the next visual/input increment.
