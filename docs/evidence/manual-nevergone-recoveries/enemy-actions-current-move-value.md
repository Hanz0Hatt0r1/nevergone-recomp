# Manual reconstruction: enemy action current move value

> Address convention: RevEngInstr executable addresses use an image base
> `0x10000` above the shipped ELF VMA. Gold-label `source_entry` keeps that
> corpus address; subtract `0x10000` to inspect the ELF.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Native function

`EnemyActionsSystem::GetCurMoveValue()` is at image address `0x002bc158`
and ELF VMA `0x002ac158`. Thumb inspection of the complete `0x2ac158..0x2ac195`
body shows an unconditional point transition:

1. Copy `EnemyActionsSystem +0x180/+0x184` to the returned `CCPoint`.
2. Set `+0x180/+0x184` to `(0,0)`.
3. Add the returned point to `+0x270/+0x274` and store that cumulative point.

There are no branches or range checks. The project contract in
`android/app/src/main/cpp/enemy_actions_get_cur_move_value.cpp` preserves the
pending point as the return value and keeps the cumulative point separately.

## Caller context

`EnemyObject::tickEnemy()` calls this method before `setEnemyMove()` in its
non-completion path. `LansquenetObject::tick()` and `PetObject::tickPet()` also
consume it. The follow-up setter's coordinate effects are outside this
contract, so the returned point is not yet applied by the tutorial renderer.

`enemy-actions-current-move-value-labels.jsonl` adds one gold function-role
example for the automatic reconstruction corpus. The RevEngInstr corpus itself
and generated datasets remain outside Git.
