# Player horizontal-facing graph gold

This consumer-owned gold subgraph covers every direct call in the captured
`MEPlayer::setControlsState` (`0032ddee`) and `MeActionsSystem::filpAction`
(`00339108`) P-code exports. The Nevergone recovery note records the two
`filpPlayer` calls and the `CCSprite::setFlipX` call as part of the shipped
facing path. The expected graph therefore has exactly three `calls` edges.

The gold rows were manually adjudicated from the Nevergone recovery note and
the cited source records; they were not generated from a Semantic Program
Graph. P-code `record_index` values are one-based data rows with the TSV header
excluded. `NEVERGONE.ProjectRecoveryNote/1` `record_index` values are Markdown
line numbers, pinned by the note's SHA-256.

This replay checks the consumer's documented call structure. It does not
validate joystick behavior, field meaning, runtime execution, or accepted-edge
confidence. The recovery note and P-code export describe the same shipped
binary, so the result is an independently curated consumer interpretation,
not independent binary or runtime corroboration.

| Source | SHA-256 |
| --- | --- |
| [Ghidra P-code rows](../../player-horizontal-facing-pcode.tsv) | `d1127243ceb7a011f9d2ac41d827c1c20e7da7bfcfef6c60729b5382f53c0ef1` |
| [Consumer recovery note](../../player-horizontal-facing-input.md) | `da64969db67461e7934d461b22e055ea3e4013f6d75627d7c749639a45033a93` |

The gold pins graph `program:6c909c5b932abf2c3a09d25c6ec072be8ab047bee480caf35a68dcb784ce4dcb` for program `nevergone-1.0.9-armv7-libcocos2dcpp`. The corresponding RevEngInstr evaluation replay reports 3/3 calls matched, 100% provenance completeness, and no accepted edges because graph confidence is uncalibrated.
