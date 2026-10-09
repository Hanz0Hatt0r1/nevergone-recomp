# SingleSelectHero career-selection state

This note records the clean-room semantic state recovered from the shipped ARMv7 `SingleSelectHero::init(bool)`, `SingleSelectHero::initUI()`, `SingleSelectHero::menuOpenGC(CCObject*)`, `SingleSelectHero::OpenTheDoor(bool,int)`, `SingleSelectHero::Carousel(int)`, and `SingleSelectHero::menuConfirm(CCObject*)` paths. Original disassembly and proprietary asset bytes are not stored in the repository.

## Recovered career domain

The class-selection scene operates on the two observed career tags:

```text
1
2
```

No third career is introduced by the recompilation.

`SingleSelectHero::init()` initializes the current/selected fields to zero. When an existing hero entry is available, the value stored in that entry's recovered `+0x3c` field is copied into the SingleSelectHero field later compared directly against career tags 1 and 2. The state layer calls this value `unavailable_career` because the matching choice is disabled by the scene; this name avoids assuming any wider save-system meaning beyond the recovered comparison.

## Initial selection

The tail of `initUI()` establishes the initial selection as:

```text
selected = 1
if unavailable_career == 1:
    selected = 2
current = selected
```

The same path disables the interaction gate before entering the initial `Carousel(selected)` presentation. `single_select_hero_state::begin()` therefore starts with input locked and a transition pending. The later presentation layer must call `complete_transition()` when the reconstructed door/carousel transition reaches the point corresponding to `OpenTheDoor(true, ...)`.

## Career changes

`menuOpenGC()` obtains the sender tag and changes selection only when the recovered interaction gate is enabled. It rejects the already selected tag and the value represented by the existing/unavailable career. On a successful change it calls `OpenTheDoor(false, ...)`, copies the sender tag into both recovered current/selected fields, and starts the carousel transition.

The semantic state mirrors this behavior:

- only careers `1` and `2` are accepted;
- input must be enabled;
- the unavailable career cannot be selected;
- selecting the already selected career is a no-op;
- a successful change locks input and marks a transition pending until `complete_transition()`.

## Confirm boundary

`menuConfirm()` reads the current career and forwards it unchanged to the downstream branch. In the online branch the same integer reaches:

```text
CharacterNameLayer::CretaUI(career)
```

and the already reconstructed CharacterNameLayer later forwards that same career to:

```text
LUA_LOGIN::CreateTheRole(name, career)
g_UILogin.CreateCharacter(name, career)
```

The confirm handler itself does not contain the `menuOpenGC()` interaction-gate check, so `single_select_hero_state::confirm()` does not invent one. Confirmation stages a generation-tagged request containing the exact current career; it deliberately does not decide whether the caller should enter the recovered offline save-creation path or the online CharacterNameLayer path.

## Current boundary

`single_select_hero_state` now provides:

- the exact two-career domain;
- recovered initial selection with an unavailable existing career;
- the interaction/transition gate used by class changes;
- scene generation and stale-confirm invalidation;
- exact current-career confirm requests;
- presentation-independent diagnostics.

Rendering the class carousel, restoring the confirmed SingleSelectHero controls/animations, hit-testing them, and routing an online confirm into `character_name_state::begin(career)` remain the next integration increments.
