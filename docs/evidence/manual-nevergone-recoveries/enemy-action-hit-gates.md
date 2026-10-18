# Manual reconstruction: enemy action hit gates

> RevEngInstr/Ghidra executable addresses use the image base `0x10000` above
> the shipped ELF VMA. Object-field offsets are unchanged.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Native action-property transfer

`EnemyObject::initActionsSystem(cocos2d::CCString*)` at ELF VMA `0x002b32a0`
reads the action configuration dictionary's `not_hit` and `go` properties,
converts them with `CCString::boolValue()`, and stores the results at
`EnemyActionsSystem +0x2b5` and `+0x2b6`, respectively. The selected keys were
identified from the original ARM literal pool. `CCDictionary::valueForKey()`
returns an empty `CCString` for a missing key; `CCString::boolValue()` returns
false for an empty value, `"false"`, or `"0"`, and true for other nonempty
values. `enemy_action_metadata_contract::native_bool_value()` models that
conversion.

The imported `enemy/enemy03/enemy_actions_1_03.plist` action entries selected
by the current tutorial route are action 47 (`e03_e_born02b.actData`) and
actions 1, 2, 3, 4, 21, 27, 28, and 43. Those dictionaries have no `not_hit`
or `go` key. Action 1's `goValue`, action 2's `runValue`, and actions 28/43's
`goValue` are separate numeric fields; they do not set the boolean `go` gate.
The native values are therefore `+0x2b5 = 0` and `+0x2b6 = 0` for the
currently selected tutorial actions.

## Runtime handoff

The tutorial action selection carries these two recovered flags. The live enemy
render queue attaches them to its current sprite command and Section-E
hitpoint candidates. After the corresponding enemy sprite frame is drawn, the
renderer reports the number of visible tutorial actions for which the native
`not_hit` gate permits collision. This records collision eligibility without
claiming that `BattleManager::playerHitEnemy()` or damage resolution is active.

The next combat boundary remains the native point-contact and damage path. It
still needs to consume the player hitpoint candidates, enemy action bounds, and
the recovered player attack value with the same timing and state gates as the
original.

Gold function-role label for `EnemyObject::initActionsSystem` is recorded in
`enemy-action-hit-gates-labels.jsonl`. The generated RevEngInstr corpus and
manual-restoration dataset remain outside Git. Adding the two gold rows to
v55 produced local dataset v56 with 9,212 labelled functions, 6,919 structural
groups, 9,839 gold label rows, and splits of 7,400 train, 819 test, and 993
validation functions.
