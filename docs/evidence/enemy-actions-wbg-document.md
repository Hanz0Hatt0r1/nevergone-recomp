# EnemyActionsData composed WBG parser

The project-owned `enemy_actions_wbg_document` layer composes the previously recovered and independently tested `EnemyActionsData::loadWBGFile` Sections A through G without adding new field semantics.

The composed order is:

1. 16-byte fixed header.
2. Section A primary variable-length records, repeated by the unsigned header count at `EnemyActionsData+0xc4`.
3. Section B compact counted records.
4. Section C nested counted groups.
5. Section D `header_word0`-gated 6/20 group fanout.
6. Section E fixed-tail records.
7. Section F fixed 24-byte tuples, repeated by the primary-record count.
8. Section G final 4-byte int table, also repeated by the primary-record count.

Each child parser remains the authority for its own evidence boundary and byte accounting. The document parser only advances by a child parser's validated `bytes_consumed` value and publishes output after every recovered section succeeds.

The document parser reports `bytes_consumed` plus `trailing_bytes` instead of requiring EOF. The original parser's policy for extra bytes or malformed files has not been proven, so rejecting otherwise well-formed recovered sections merely because bytes remain would invent behavior.

The composed parser still does not assign gameplay meanings to structural integer/string fields, combo branch values, or unresolved `ActionFrameData` members. Validation against genuine user-owned WBG payloads remains a separate dynamic-evidence step.
