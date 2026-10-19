# Manual reconstruction: enemy combat hit-point origin

> RevEngInstr addresses use an image base `0x10000` above the shipped ELF VMA.
> The original artifact is Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
> `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Native position update

The local Ghidra record for `EnemyObject::enemyHLPos()` is at `0x002bf122`
(ELF VMA `0x002af122`). It first refreshes the point at `EnemyObject +0x17c`
from `enemyBoundingBox()` and subtracts `EnemyActionsSystem::GetFrameYOffset()`
from its Y value when the action-system pointer at `+0x10c` is present. It then
selects the combat origin at `+0x184`:

- states `0x0f` and `0x11` retain the previous origin;
- with `EnemyObject +0x248 != 1`, states `0x1c`, `0x2e`, and `0x2f` use the
  object position at `+0x280`;
- all other states use the refreshed `+0x17c` point.

`EnemyObject::setEnemyPos()` stores the supplied spawn `CCPoint` at `+0x280`
and `+0x288`. `EnemyObject::initWithID()` initializes `+0x248` and byte `+0x31f`
to zero and stores the supplied point at `+0x280`. The `+0x2a0` field is the
enemy ID passed through `BattleManager::addEnemyObject()`. Thus enemy ID 3 with
state `0x1c` and `+0x248 == 0` has an exact combat origin equal to its spawn
point; its `isFly()` predicate is also false.

## Why the tutorial state keeps `+0x248` clear

The existing reviewed enemy-action table selects action 28 for enemy 3 state
`0x1c`. The original `enemy_actions_1_03.plist` in the installed asset cache
selects `e03_e_w01_back.actData` and gives action 28 `goValue = 3`. In
`EnemyActionsSystem::initActionsSystem()`, this integer is stored at action
system `+0x2dc`. `EnemyObject::setEnemyState()` copies that value into its
`+0x248` mode only for values 0, 1, and 2; value 3 leaves the initialized zero
unchanged. This matches the modeled initial tutorial state without assuming a
movement offset. The extracted plist SHA-256 is
`02571079e0e451c11a1c7f2b826d4e6ce86163a3b3e0103bb3907b32ff911a7f`; the
selected action WBG SHA-256 is
`9548a24ec5075d2df878c2a3729b798bc0bb4520c01fd1b593f53cd91f5a343c`.

## Live collision connection

`game_scene_player_enemy_collision::resolve_attack_motion()` now builds
eligible enemy origins only from materialized, generation-matched enemy 3
bindings that remain in state `0x1c`. It uses the native player `+0x130` point
published after a successful draw, checks `BattleManager::playerIntersectsEnemy`
with the proposed movement X, and applies its recovered one-sided boundary
correction only when `MEPlayer::isCollisionState()` permits it. The predicate's
`+0x3e8` disable input is false here because the reconstructed tutorial has no
incoming-hit writer for that field; restore that input with player damage. The
point must match the current movement spawn, action state, and prior motion
tick; a stale render snapshot cannot move the player. Unsupported enemy states
fail closed.

The corrected delta feeds the existing fixed-tick player motion cursor. The
joystick report exposes collision checks, contacts, and applied corrections.
This restores the initial Attack-wave body-contact response. The bounded
`EnemyObject::actionTargetPos()` branch logic is now reconstructed separately;
its caller-owned target point and random-state inputs are not yet connected to
the tutorial actor, so live AI movement, player attacks, enemy damage, and
state-specific bounding origins remain separate work. See
[`enemy-object-action-target-position.md`](enemy-object-action-target-position.md).

Function-role labels are recorded in `enemy-hl-position-labels.jsonl`. The
four evidence-backed gold rows were appended to the local labels, then rebuilt
as `dataset-manual-restoration-v57`: 9,215 labeled functions, 6,922 structural
groups, 9,843 label rows, with 7,402 train, 819 test, and 994 validation
functions. The original binary, action plist, action WBG, and generated
reconstruction dataset remain outside Git.
