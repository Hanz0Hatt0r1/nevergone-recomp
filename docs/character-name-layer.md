# CharacterNameLayer role-creation contract

This note records clean-room behavior recovered from the shipped ARMv7 `CharacterNameLayer::CretaUI(int)`, `CharacterNameLayer::menuCloseCallback(CCObject*)`, `SingleSelectHero::menuConfirm(CCObject*)`, and `LUA_LOGIN::CreateTheRole(std::string const&, int)` paths. Original disassembly and proprietary asset bytes are not stored in the repository.

## Career identity

`SingleSelectHero::menuConfirm()` reads its selected value and, on the online path, passes it directly to `CharacterNameLayer::CretaUI(selectedCareer)`.

`CretaUI(int)` immediately stores that argument at the CharacterNameLayer career field. The submit callback later reads the same field and passes it unchanged to:

```text
LUA_LOGIN::CreateTheRole(name, career)
```

`CreateTheRole` in turn forwards the same integer unchanged to the imported Lua login function:

```text
g_UILogin.CreateCharacter(name, career)
```

The recompilation therefore treats the CharacterNameLayer creation parameter as `Career` itself; there is no reconstructed mapping table between the two values.

## Recovered callback tags

`CharacterNameLayer::menuCloseCallback()` resolves the sender tag and recognizes these controls:

- tag `1`: validate and submit the current edit-box name;
- tag `2`: close/dismiss the CharacterNameLayer and its edit-box/IME path;
- tag `3`: generate a random name from `RandomName.csv` and write it into the edit box.

The edit box is attached as child tag `102`.

At the end of `CretaUI(int)`, the shipped implementation calls the callback with the random-name control, so a newly created CharacterNameLayer immediately performs the tag-3 random-name action. `character_name_state::begin(career)` preserves this as an initial pending randomize request.

## CharacterNameLayer validation boundary

The online CharacterNameLayer submit path is distinct from the legacy ChooseHero tag-4 submit path.

For CharacterNameLayer the shipped callback:

1. rejects an empty string through the original prompt path;
2. calls `ManagementLayer::LGG_CheckStringLegal(...)`;
3. measures the validated UTF-8 buffer with `strlen`;
4. accepts values whose byte length is `<= 18`;
5. calls `LUA_LOGIN::CreateTheRole(name, career)` when valid.

The previously recovered legacy `ChooseHero::OnCreateback(tag=4)` path uses a `<= 21` byte boundary. The recompilation keeps both contracts instead of silently forcing one limit onto both screens.

## Confirmed presentation resources

The recovered CharacterNameLayer construction references the following imported resources/localization keys:

- `Login/ChooseHero/Redbottom.png`;
- `ServerList/RANDOMName.png`;
- `ServerList/RANDOM.png`;
- `Button_C_a.png` / `Button_C_b.png`;
- `TouMing.png` as the scale-9 edit-box background;
- localization keys `Prompt9`, `Confirm`, and `Cancel`;
- font family `Arial` at the recovered button/title sites.

These names are evidence for the later renderer/input reconstruction. No replacement artwork is embedded in the repository.

## Current implementation boundary

`character_name_state` now owns the presentation-independent semantics:

- active scene generation and exact career identity;
- current edit-box text;
- exact tag `1` / `2` / `3` action mapping;
- initial and explicit random-name requests;
- CharacterNameLayer-specific 18-byte validation;
- successful submit staging through `role_selection_state::request_create_role(name, career)`.

The existing persistent login Lua executor remains responsible for consuming that staged request and calling `g_UILogin.CreateCharacter(name, career)`. RandomName.csv parsing, Android text/IME presentation, visual composition, and the SingleSelectHero class-selection controls are separate follow-up increments.
