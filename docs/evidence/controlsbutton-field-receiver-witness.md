# ControlsButton field receivers: exact native witness (P1A, 2026-10-14)

## Source and scope

Evidence inputs: original `com.hippiegame.nevergone.apk`, its
`lib/armeabi-v7a/libcocos2dcpp.so`, and the eight-call Ghidra
`NEVERGONE.GhidraStructuralReceiverSlice/1` TSV for
`Controls::showControlsAttackKey(int)`. Exact original ELF SHA-256:
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

The Ghidra address view adds `0x10000` to ELF virtual addresses.
The static witness checker is
`tools/ml/audit_nevergone_controlsbutton_field_receiver_witness.py`.
It accepts the original APK or extracted `.so` locally and produces
**only JSONL/text evidence**, never copies binary data to the repository.

## Receiver load sites (actual CALLIND object, not caller argument)

| Call | Ghidra CALLIND | Receiver field | Ghidra receiver LOAD |
| --- | --- | --- | --- |
| 0 | `002a75b4` | `Controls + 0x12c` | `002a75aa` |
| 1 | `002a75c2` | `Controls + 0x130` | `002a75b6` |
| 2 | `002a75d0` | `Controls + 0x134` | `002a75c4` |
| 3 | `002a7612` | `Controls + 0x134` | `002a7608` |
| 4 | `002a7634` | `Controls + 0x130` | `002a762a` |
| 5 | `002a764a` | `Controls + 0x12c` | `002a763e` |
| 6 | `002a7658` | `Controls + 0x134` | `002a764c` |
| 7 | `002a7686` | `Controls + 0x12c` | `002a767c` |

All eight raw Ghidra slices contain `LOAD -> CAST -> INT_ADD` with a
single shared register root and the expected constant offset. Each
has five nodes, no truncation and a canonical receiver. The original
Thumb instructions independently verify the field load, vtable slot
`0x80` load and `BLX r3` at every callsite.

## Constructor-linked store witnesses

`Controls::init()` calls
`ControlsButton::create(char const*, char const*, cocos2d::CCObject*, callback)`
at ELF `0x2985fc` and stores its return value to these fields:

| Field | ELF factory BL | ELF field STR | Ghidra field STR |
| --- | --- | --- | --- |
| `0x12c` | `0x298118` | `0x298120` | `002a8120` |
| `0x130` | `0x29816a` | `0x298172` | `002a8172` |
| `0x134` | `0x2981bc` | `0x2981c2` | `002a81c2` |

The checker pins the complete Thumb byte span from each factory BL
through the matching STR, including non-clobbering intervening
instructions. This is stronger than matching a call and an unrelated
store in the same function.

The factory at ELF `0x2985fc` calls the
`ControlsButton::ControlsButton()` constructor at ELF `0x298448`
(from `0x298614`). The constructor loads a GOT pointer at
ELF `0x92ee20`, which has a single `R_ARM_RELATIVE` relocation and
contains `0x8e46f0` (`_ZTV14ControlsButton`). It writes the vtable
address point `0x8e46f8` into the object. That address point is
Ghidra vtable ID `libcocos2dcpp.so:008f46f8`.

The vtable entry at ELF `0x8e46f8 + 0x80 = 0x8e4778`
has a single `R_ARM_RELATIVE` relocation and stores the Thumb
function pointer `0x515d2f`. This resolves to
`cocos2d::CCNode::setVisible(bool)` at ELF `0x515d2e`,
Ghidra target ID `libcocos2dcpp.so:00525d2e`.

The direct non-virtual calls to
`ControlsButton::unselected()` in the same `showControlsAttackKey`
function corroborate the intended field class; they do not replace
the constructor/store proof.

A bounded halfword-aligned scan of the original Thumb code region
`0x2966d4..0x2983b0` (the contiguous `Controls` method block)
finds **exactly three** direct `STR.W Rt,[Rn,#imm12]` writes to
`0x12c`, `0x130`, `0x134`: the three factory stores above,
all with `Rn=r4`, `Rt=r0`. This scan does **not** exclude
register-indexed stores, aliases or writers outside the bounded block.

## Conservative conclusion

The actual receiver values of all eight virtual calls are loaded
from three `Controls` fields **initialized by `ControlsButton::create`**.
For the constructor-installed `ControlsButton` vtable, slot `0x80`
converges to `CCNode::setVisible(bool)`.

**Not yet proven:** all writes to these three fields across the
object's lifetime, including aliased stores and post-initialization
replacement, have not been excluded. Therefore the new audit status
is `constructor_linked_field_receiver_candidate` and
`receiver_class_proven=false`. No v11 recovery query or production
routing change is authorized by this witness alone. The old
caller-argument class attribution remains quarantined.

## Reproduce on Manjaro

After refreshing v11 and running the mixed receiver slice probe:

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only

bash tools/ml/run_nevergone_mixed_slot_target_recovery_v11.sh
bash tools/ml/run_nevergone_mixed_receiver_value_probe.sh

bash tools/ml/run_nevergone_controlsbutton_field_receiver_witness.sh \
  /path/to/com.hippiegame.nevergone.apk

WORK="$HOME/.cache/nevergone-recomp/receiver-provenance"
cat "$WORK/controlsbutton-field-receiver-witness.txt"
```

The checker is fail-closed on the exact ELF hash, symbols,
constructor-vtable relocation chain, all three factory-to-store byte
spans, all eight native virtual dispatch sequences, all eight
Ghidra receiver trees and current v11 inference identities.
