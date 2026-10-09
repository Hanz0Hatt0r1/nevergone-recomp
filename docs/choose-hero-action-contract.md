# ChooseHero `OnCreateback` action contract

This note records clean-room behavior recovered from the shipped ARMv7 `ChooseHero::menuCreateback()` / `ChooseHero::OnCreateback(int)` path. It documents semantic calls and field relationships only; original disassembly is not stored in the repository.

## Dispatch boundary

`ChooseHero::menuCreateback(CCObject*)` obtains the sender tag through the Cocos node virtual API and forwards that integer directly to `ChooseHero::OnCreateback(int)`.

The shipped handler recognizes tags `1..8`. Values outside that range fall through without one of these recovered actions.

## Recovered tags

### Tag 1 — open create-hero screen

Calls `ChooseHero::CreatHoreScreen(...)` with the current create-role parameter stored by the ChooseHero object. This is the entry into the character-creation presentation rather than the final role submission.

### Tag 2 — leave ChooseHero for select-hero UI

The shipped path:

- hides the ChooseHero pane;
- hides player/control UI;
- calls `ManagementLayer::createSelectHero(false)`;
- updates application-mode fields;
- conditionally restarts the configured background music.

The recompilation names this semantic action `leave-to-select-hero`; it does not reproduce the unresolved application-mode field layout in the action-state layer.

### Tag 3 — start the selected existing hero

The selected standalone id is read from the ChooseHero current-hero field. The shipped path has two runtime branches around a `ManagementLayer` state flag:

- one branch finds the matching `SaveDataHero`, copies it into ManagementLayer state and calls `LUA_LOGIN::lua_SelectCharactersStartTheGame(...)`;
- the other creates/configures the local `MEPlayer`, creates its `DataManager` data, clamps the level to the application limit, updates the HP-data level, then calls `ManagementLayer::GoToGameLayer(...)` and starts game timing.

Both branches are downstream execution work. `choose_hero_action_state` records the selected id and keeps execution separate so the reconstructed offline route can implement the local branch without pretending the legacy service branch already exists.

### Tag 4 — submit create-role name

The name is obtained from the ChooseHero `CCTextFieldTTF`. The shipped path:

1. handles an empty string with a two-stage message instead of submitting;
2. validates the text through `ManagementLayer::LGG_CheckStringLegal(...)`;
3. measures the validated UTF-8 buffer with `strlen`;
4. accepts at most `21` bytes;
5. calls `LUA_LOGIN::CreateTheRole(name, createRoleParameter)` when valid.

The action state records both `role_name` and `create_role_parameter`; validation/execution remains a separate executor boundary.

### Tag 5 — randomize role name

Opens `RandomName.csv`, selects random components and writes the generated result into the same text field. The field mutation is the normal `CCTextFieldTTF::setString(char const*)` path.

### Tag 6 — detach IME and restore save list

Calls `CCTextFieldTTF::detachWithIME()` and then rebuilds the existing-save presentation through `ChooseHero::initSaveDataUI()`.

### Tag 7 — attach IME

Calls `CCTextFieldTTF::attachWithIME()`.

### Tag 8 — confirm deletion of selected hero

Builds a two-button confirmation frame. The recovered member-function target used by that dialog resolves to `ChooseHero::callback_deleteHero(CCObject*)`.

This action therefore means `confirm-delete-selected-hero`; the action-state layer does not delete a save by itself.

## `CCTextFieldTTF` virtual slots

The shipped ChooseHero object stores its text field at its recovered field boundary. Calls made through that object's vtable resolve to:

- virtual offset `0x218` -> `CCTextFieldTTF::setString(char const*)`;
- `0x21c` -> `CCTextFieldTTF::getString()`;
- `0x220` -> `CCTextFieldTTF::attachWithIME()`;
- `0x224` -> `CCTextFieldTTF::detachWithIME()`.

These resolutions are confirmed against the shipped `CCTextFieldTTF` vtable rather than inferred from the surrounding UI behavior.

## Reconstructed state boundary

`choose_hero_action_state` provides a small scene-local request queue boundary:

- exact tag-to-action mapping for `1..8`;
- scene generation;
- selected hero id;
- create-role parameter;
- role-name text;
- monotonic dispatch/consume counters;
- non-destructive `peek()` and explicit `consume()` semantics;
- stale-request reset on a new recovered scene generation.

It intentionally performs no `ManagementLayer`, `MEPlayer`, Lua, IME, random-name, or delete side effects yet. The next execution layer can consume only the actions whose dependencies have been reconstructed, while leaving unsupported legacy-service branches visible in diagnostics instead of silently faking success.
