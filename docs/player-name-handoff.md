# Player name handoff

The project-owned player spawn resolves its name from the role selected or
created on the login path:

1. A committed existing-role entry supplies its exact `CharacterName` from the
   `EnterGameWithCid` handoff.
2. A freshly created role supplies the name stored by `CharacterNameLayer` and
   submitted through `g_UILogin.CreateCharacter(name, career)`.
3. `SELF` remains the original bootstrap fallback before a role name exists.

The player spawn snapshot and rendered player command retain this resolved
identity, and tutorial dialogue reads from the same resolver. This keeps the
spawned character and its dialogue label aligned with the selected role without
deriving names from Career ids or inventing a Career-to-name mapping.
