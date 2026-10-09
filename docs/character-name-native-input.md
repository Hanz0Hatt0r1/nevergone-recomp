# Native CharacterName input

This increment gives the reconstructed CharacterName modal first refusal in the existing `nativeOnServerSelectionTouch` path.

## Recovered controls

The router reuses `character_name_layout` and the exact staged resource dimensions. It does not introduce guessed hit boxes. Recovered callback tags remain the semantic boundary:

- tag 1: Confirm / create role
- tag 2: Cancel / close CharacterName
- tag 3: Random / generate a name

Completed gestures dispatch through the already reconstructed `character_name_action_executor`, so validation, Lua create dispatch, close behavior and RandomName handling remain centralized.

## Coordinates and ownership

Android input is top-origin. The router uses the same 1136×640 aspect-fit transform as the GLES CharacterName presentation and converts touches back to bottom-origin design coordinates before calling `character_name_layout::contains`.

DOWN/POINTER_DOWN owns one pointer. MOVE only keeps a control pressed while that pointer remains inside its originally armed hit box. UP/POINTER_UP dispatches only when released inside the same control. CANCEL clears ownership. Other pointers are consumed while CharacterName is modal.

The recovered full-surface transparent `Button_C` blocker is represented by consuming background touches before any older scene router sees them. If native assets are unavailable, active CharacterName still consumes the game surface while the external Android compatibility controls remain usable.

## Compatibility boundary

The Android `EditText` remains the temporary IME/text-entry path. The external compatibility Random/Confirm/Cancel buttons are still retained for fallback/accessibility during this increment; native game-surface actions now perform the same semantic dispatch. A later presentation step can bind `pressed_tag()` to the already staged fixed-button pressed frame and then remove redundant external action buttons.
