# SingleSelectHero online career-selection contract

This note records clean-room behavior recovered from the shipped ARMv7 `SingleSelectHero::init(bool)`, `SingleSelectHero::initUI()`, `SingleSelectHero::menuOpenGC(CCObject*)`, `SingleSelectHero::OpenTheDoor(bool,int)`, and `SingleSelectHero::menuConfirm(CCObject*)` paths. Original disassembly and proprietary asset bytes are not stored in the repository.

## Supported careers

The recovered selector presentation is built around exactly two career values:

- career `1`;
- career `2`.

`Carousel(int)` has explicit presentation branches for values up to `2`, and the initial-selection logic chooses only `1` or `2`.

## Existing-career input and default selection

During `init(bool)`, SingleSelectHero reads the first existing role record from ManagementLayer state when one is present and stores the recovered value later compared against the candidate career.

`initUI()` then performs this selection setup:

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

## `menuOpenGC` selection and transition gate

The class buttons carry their career directly as the sender tag. `menuOpenGC()` checks the recovered interaction byte before accepting a class change. The same byte is written by `OpenTheDoor(bool, ...)`:

- `OpenTheDoor(false, ...)` -> interaction locked while the door/carousel transition is active;
- `OpenTheDoor(true, ...)` -> interaction enabled again.

After that gate permits interaction, `menuOpenGC()`:

1. reads the sender tag;
2. ignores the click when the same career is already selected;
3. starts the changed-career door/carousel transition with interaction locked;
4. writes the sender tag unchanged into the selected-career fields.

`single_select_hero_state::begin()` mirrors the exact `initUI()` boundary by starting with input locked and a transition pending. The later visual transition executor calls `complete_transition()` at the recovered unlock point. A successful changed-career selection locks input again until the next transition completes.

The recovered `menuOpenGC()` handler itself does not compare the sender tag against the existing-career field, so the semantic state does not invent that rejection there. The proven existing-career equality block remains in `menuConfirm()`.

## Online confirm path

`menuConfirm()` compares the current candidate career against the existing role's career. If they are equal, the shipped function returns without entering role creation.

For the online ManagementLayer branch, a valid different career follows this path:

```text
selectedCareer
  -> CharacterNameLayer::create()
  -> CharacterNameLayer::CretaUI(selectedCareer)
```

The same integer later reaches `LUA_LOGIN::CreateTheRole(name, career)` and `g_UILogin.CreateCharacter(name, career)` through the already reconstructed CharacterNameLayer contract.

The recovered `menuConfirm()` path does not test the `menuOpenGC` interaction byte. `single_select_hero_state::confirm_online()` therefore does not add an artificial transition-gate requirement; it opens `character_name_state` with the selected career unchanged and records blocked existing-career confirms separately.

## Current implementation boundary

`single_select_hero_state` now owns the non-visual semantics:

- active scene generation;
- existing career;
- exact default selection rule;
- valid careers `1` and `2`;
- recovered `OpenTheDoor` input/transition gate for class changes;
- sender-tag selection behavior;
- existing-career confirm blocking;
- online confirm bridge into `character_name_state::begin(career)`;
- diagnostics counters.

The current `single_select_hero_compositor` still renders only the previously reconstructed background/opening presentation. Rendering the two career choices, completing the gate at the correct animation point, recovering their hit boxes and wiring touch into this state are the next visual/input increment.
