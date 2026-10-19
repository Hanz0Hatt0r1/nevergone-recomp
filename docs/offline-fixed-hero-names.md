# Offline fixed hero names and two save slots

The standalone Never Gone route uses **two hero identities**, not editable
player-entered profile names.

The existing shipped save slots are:

| Hero | Save file | Name localization key | English |
| --- | --- | --- | --- |
| 1 | `DMG_01.sData` | `Hero1Name` | DAWN |
| 2 | `DMG_02.sData` | `Hero2Name` | HIGGS |

These keys, slot files, and presentation semantics were already recovered in
[ChooseHero standalone save metadata](choose-hero-save-metadata.md).

## Offline creation flow

The shipped original also contains a legacy online CharacterName/CreateCharacter
path, including random-name and editable-name callbacks. The reconstructed
standalone flow previously exposed that compatibility edit box. It is now
treated separately:

1. The player chooses a visible hero (1 or 2) in SingleSelectHero.
2. Native creation resolves `Hero%dName` from the imported
   `assets/Login/ALL_Loin.csv` using the original language-column mapping.
3. It checks the corresponding `DMG_0%d.sData` file **at creation time**.
   If the file exists, the hero is already created and the attempt is rejected.
4. The chosen hero label is staged as the role name. The pending random-name
   callback is explicitly cleared, and the existing CreateCharacter dispatch
   and offline tutorial bootstrap run unchanged.
5. The Android compatibility overlay does **not** display a name input or
   keyboard on this offline path. Errors expose Retry and Back controls.

Deleting a hero removes its corresponding save-file occupancy when the
application's delete/save lifecycle performs that deletion. A subsequent
creation request probes the current files again and can recreate that hero.
The new feature does not itself delete, overwrite, or manufacture DMG saves.

The legacy online route still uses the recovered CharacterName editing state;
it is outside the scope of this standalone behavior change.

## Validation

`tools/offline_hero_creation_smoke.cpp` exercises fixed localized name
selection, rejection of hidden careers 3..5, occupied-slot rejection, both
slots occupied, deletion/recreation, and fail-closed behavior for missing
localization. The smoke is invoked by the fast GitHub workflow.

On-device tests still need to verify end-to-end creation, successful persistence
of `DMG_01.sData`/`DMG_02.sData`, and the existing delete confirmation
handler. This PR does not claim to implement the entire missing save deletion
execution path.
