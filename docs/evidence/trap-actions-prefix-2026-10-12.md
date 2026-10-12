# TrapActionsData fixed prefix — 2026-10-12

## Clean-room scope

`SceneActionsSystem::initWithFile()` in the shipped Never Gone 1.0.9 ARMv7 library resolves the selected action filename and calls `TrapActionsData::createWithFile(file, true)`. This note records only the first fixed reads performed by `TrapActionsData::loadWBGFile()`; no original action data or disassembly listing is committed.

## Fixed prefix

Before entering its first record loop, `TrapActionsData::loadWBGFile()` reads exactly:

| File offset | Width | Project structural field |
| ---: | ---: | --- |
| `0x00` | 4 | signed int32 `first_i32` |
| `0x04` | 4 | float32 `first_float` |
| `0x08` | 4 | float32 `second_float` |
| `0x0c` | 4 | uint32 `record_count` |

The fourth value is copied to `TrapActionsData +0xbc` and directly controls the following main record loop. The first record therefore begins at absolute offset `0x10`.

## Project parser

`trap_actions_prefix::parse()` reconstructs only this proven 16-byte boundary. It uses the shared bounded `hp_data::Cursor`, leaves the caller output unchanged on truncation, and rejects non-finite float values before later runtime code can consume malformed imported data.

The two float semantics and the first signed integer remain intentionally structural until their downstream uses are separately proven. No arbitrary upper bound is assigned to `record_count` at this parsing layer; allocation/runtime consumers must impose their own resource budgets when they begin materializing records.

## Next boundary

The first TrapActions record is variable-length and begins at `0x10`. Static inspection already shows a fixed field prefix followed by length-prefixed strings and later nested arrays. That record layout should be reconstructed transactionally before `klhuo` SceneActions playback is enabled. Until then selected `bornNN/iNN.actData` can be identified but not treated as a fully supported action document.
