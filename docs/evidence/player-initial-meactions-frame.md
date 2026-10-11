# Initial action28 player frame: corrected native path

The local Ghidra program is the analyzed ARMv7 `libcocos2dcpp.so` with SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.
The bundled headless probe was run against the project's nested program folder
`/com.hippiegame.nevergone.apk/lib/armeabi-v7a/libcocos2dcpp.so`.
Its derived outputs are [`player-current-frame-stores.tsv`](player-current-frame-stores.tsv)
and [`player-current-frame-summary.txt`](player-current-frame-summary.txt).

The probe found two `EnemyActionsSystem +0x294` stores: a derived value in
`startFrameIdx` at `0x002bbe8c`, and literal zero in `play(int,float)` at
`0x002bda18`. Neither is the first `MeActionsSystem` player's current-frame
field. `MeActionsSystem` is a separate class with its action data at `+0x110`,
primary-frame array at `MeActionsData +0x34`, and current frame at `+0x1a4`.
The prior bridge incorrectly applied the enemy readiness byte `+0x26c` and
enemy current-frame offset `+0x294` to this player path.

Bounded decompilation of the corresponding named functions establishes:

1. `MEPlayer::createActions` at `0x00328d8c` constructs each action with
   `MeActionsSystem::creinitWithFile` at `0x00329092`; the existing
   `MEPLAYERSTATE 0x1b -> action28` selector applies to that construction.
2. `MeActionsSystem::initWithFile` at `0x0033bd10` writes literal zero to
   `this+0x1a4` before it reads the first primary record at `MeActionsData+0x34`
   and requests its frame name at `ActionFrameData+0x68`.
3. `MEPlayer::createActions` invokes `MeActionsSystem::play()` at `0x003293b4`
   for the initially selected action. `MeActionsSystem::play()` at
   `0x0033b5d8` writes literal zero to `this+0x1a4`, selects index zero from
   `MeActionsData+0x34`, then calls `MeActionsSystem::updateData()` at
   `0x0033b68c`.
4. `MeActionsSystem::updateData()` at `0x00339d34` selects the same array by
   `this+0x1a4` and requests a sprite frame by the selected record's `+0x68`
   name. Its frame-name length is at `+0x6c`.

The zero seed is therefore specific to the initial player action path and
precedes the first `updateData` lookup. The project-owned player bridge now
uses an explicit `current_frame_1a4` state and a validated primary-record frame
key. Loading a parsed action document is the readiness boundary; it does not
reuse `EnemyActionsSystem +0x26c`.

One user-owned player `.actData` from the local OBB (not committed) consumed
exactly 55,169 of 55,169 bytes in the existing A-G structural parser and had
28 primary records. The OBB contains `p01_actions.plist/action28` naming
`p01_i01.actData`, but that exact file is absent from this OBB. The runtime
therefore remains fail-closed until a user imports the corresponding action
file and player atlases. No original binary, plist, texture, or action payload
is included in this change.
