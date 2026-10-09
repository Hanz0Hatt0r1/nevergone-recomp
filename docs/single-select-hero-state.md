# SingleSelectHero online career-selection contract

This note records clean-room behavior recovered from the shipped ARMv7 `SingleSelectHero::init(bool)`, `SingleSelectHero::initUI()`, `SingleSelectHero::menuOpenGC(CCObject*)`, `SingleSelectHero::OpenTheDoor(bool,int)`, and `SingleSelectHero::menuConfirm(CCObject*)` paths. Original disassembly and proprietary asset bytes are not stored in the repository.

## Supported careers

The shipped selector creates five `menuOpenGC` menu items. The construction loop assigns sender tags `1` through `5` unchanged and formats their normal/highlight resources as:

```text
xrfuwen%02d.png
xrfuwenfaguang%02d.png
```

The recovered career domain is therefore exactly `1..5`. This supersedes the earlier partial reconstruction that modeled only careers 1 and 2.

The menu-item centers are built on the common 1136x640 design surface with:

```text
x = visibleWidth * 0.5
y = visibleHeight - 55 - 90 * career
```

which yields centers `(568,495)`, `(568,405)`, `(568,315)`, `(568,225)`, and `(568,135)` for careers 1 through 5. Actual hit rectangles remain dependent on the imported normal-frame content sizes and are intentionally handled by the later input/layout layer rather than guessed here.

## Existing-career input and default selection

During `init(bool)`, SingleSelectHero reads the first existing role record from ManagementLayer state when one is present and stores the recovered value later compared against the candidate career. Valid existing careers `1..5` must therefore be preserved; normalizing careers 3, 4, or 5 to zero would break the proven equality block in `menuConfirm()`.

`initUI()` performs this initial selection setup:

```text
selectedCandidate = 1
if existingCareer == 1:
    selectedCandidate = 2
selectedCareer = selectedCandidate
Carousel(selectedCandidate)
```

Therefore career 1 is the normal initial candidate, with career 2 used only when career 1 is already present. Existing careers 2 through 5 do not change that initial candidate.

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

The ARMv7 ChooseHero submit path and the shipped login Lua both preserve this integer unchanged: it is the same career value later passed to `LUA_LOGIN::CreateTheRole(name, career)` and `g_UILogin.CreateCharacter(name, career)`.

The recovered `menuConfirm()` path does not test the `menuOpenGC` interaction byte. `single_select_hero_state::confirm_online()` therefore does not add an artificial transition-gate requirement; it opens `character_name_state` with the selected career unchanged and records blocked existing-career confirms separately.

## Current implementation boundary

`single_select_hero_state` owns the non-visual semantics:

- active scene generation;
- preserved existing career for values `1..5`;
- exact default selection rule;
- valid careers `1..5`;
- recovered `OpenTheDoor` input/transition gate for class changes;
- sender-tag selection behavior;
- existing-career confirm blocking for every supported career;
- online confirm bridge into `character_name_state::begin(career)`;
- diagnostics counters.

The current compositor already renders the reconstructed background and HeroTable presentation. Remaining presentation/input work includes the five `xrfuwen` career controls, the recovered transition/unlock timing, character/overlay sprites, and the confirm control/hit box.
