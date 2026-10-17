# Manual reconstruction: live player hit-point anchor

> RevEngInstr procedure addresses use an image base `0x10000` above the shipped
> ELF VMA. The binary identity is Never Gone 1.0.9 ARMv7
> `libcocos2dcpp.so`, SHA-256
> `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Native field evidence

The player-index accessor at ELF `0x00317a2c` is a single load from
`MEPlayer +0x144`. Its setter at ELF `0x00324e94` stores the supplied index to
the same field. That field therefore selects the two state-gate tables in
`MEPlayer::updateHLValue()`; it is not an unrelated mode or boolean.

`MEPlayer::getPlayerPos()` at ELF `0x00317f78` returns the sum of the point at
`+0x240` and the scene/live offset at `+0x248`. `setPlayerPos()` at ELF
`0x003187e8` stores its point argument at `+0x248`, sets the active action
system position to the same sum, reads that node position back into `+0x268`,
and marks the player position initialized. The reconstructed spawn supplies
`+0x240`. The renderer's current player-node position includes the recovered
movement and camera transform; removing only the camera translation gives the
live `+0x268` point. The `+0x248` scene/live offset is that point minus the
bootstrap value.

`MeActionsSystem::actionBoundingBox()` at ELF `0x00329350` returns the primary
sprite's transformed bounding box or a unit rectangle if its sprite is null.
For the reconstructed live sprite, the same four corners already used by the
renderer are transformed from the complete untrimmed source frame using the
current frame's position, anchor, scale, and rotation. Their enclosing AABB is
passed to `player_hl_point_contract`, which then mirrors
`MEPlayer::playerBoundingBox()` and `updateHLValue()`.

## Runtime connection

After a frame is successfully drawn, `game_scene_direct_renderer` publishes
the current action rectangle, player rectangle, and `playerHLPoint` in
`game_scene_player_hl_point_state`. The native selector is the selected player
career because `setPlayerIdx()` writes that exact career value to `+0x144`.
The movement-bounds cache is retained for the current spawn and player index,
and is discarded when either changes. It is also invalidated before any frame
that cannot be drawn, so consumers cannot mistake an older rendered point for
the current one.

This connects the recovered point chain to the live player sprite and exposes
the result in the renderer diagnostics. Player/enemy attack resolution still
needs to consume this published point and live enemy-origin points; this
change does not claim that melee collision is active yet.

Function-role labels for `playerIdx`, `setPlayerIdx`, `getPlayerPos`, and
`setPlayerPos` are recorded in
`player-hitpoint-live-renderer-labels.jsonl`. The generated corpus and dataset
remain outside Git. Local manual-restoration dataset v49 now contains 9,203
labelled functions, 6,910 structural groups, and 9,825 gold label rows, with
train/test/validation splits of 7,393/818/992.
