# Enemy03 AI action routes — 2026-10-17

Target: Never Gone 1.0.9 ARMv7
`libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note uses the existing read-only Ghidra export produced with RevEngInstr.
The export is saved at
`/home/pes/.local/share/ghidra-decompiler/workspace/nevergone-camera-follow.jsonl`;
it is local analysis evidence and is not part of the source repository.

## Native state routes

`EnemyObject::eventAction(float)` at image address `0x002c1b28` calls
`enemy03AICPP::enemy03_AI` for enemy type 3. Its state-selection branches
include:

| State | Native evidence | Action-table entry | Recompiler result |
| ---: | --- | --- | --- |
| 27 (`0x1b`) | `eventAction`, line 536 | `e03_e_s08.actData`, one-shot | supported |
| 36 (`0x24`) | `eventAction`, line 464 | no entry | unsupported |
| 42 (`0x2a`) | `eventAction`, line 333 | no entry | unsupported |
| 43 (`0x2b`) | `eventAction`, line 460 | `e03_e_w02.actData`, loop | supported |

`EnemyObject::onlineEnemyState` uses the requested state as the action-array
index (see
[`online-enemy-action-index-2026-10-17.md`](online-enemy-action-index-2026-10-17.md)).
The action table maps states 27 and 43 to the listed assets and uses the
`e03_default.plist` atlas. The `e03_e_s08.actData` clip has 17 primary frames
(6,843 bytes); `e03_e_w02.actData` has 12 primary frames (4,848 bytes). Both
files passed the project `enemy_actions_wbg_validate --require-eof` parser
with zero trailing bytes. They are read from the user-imported original OBB at
runtime and are not copied into this repository.

## Scope

The action selector now resolves these two additional original state IDs, and
the existing renderer can sample their clips when a live binding reports that
state. This does not execute `enemy03_AI` or synthesize state transitions; the
native movement/AI loop is still outside the reconstructed runtime. States 36
and 42 remain unsupported because no corresponding action-table entries were
found. No behavior is inferred for those states.
