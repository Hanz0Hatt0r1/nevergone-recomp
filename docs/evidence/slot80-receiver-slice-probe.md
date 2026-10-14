# P1A: unresolved slot `0x80` receiver slices (v11)

## Input evidence (2026-10-14)

A user-provided v11 failure export contained 62 inference rows, 27 unresolved
virtual calls, and 17 still-unresolved slot `0x80` calls in exactly five
functions:

| Source | Calls | Status |
| --- | --- | --- |
| `libcocos2dcpp.so:002a74c0` | 0, 1, 2, 3 | unresolved receiver class |
| `libcocos2dcpp.so:002a7594` | 0, 1, 2, 5, 6 | unresolved receiver class |
| `libcocos2dcpp.so:002a7696` | 0, 1 | unresolved receiver class |
| `libcocos2dcpp.so:002a78cc` | 0, 1, 2 | unresolved receiver class |
| `libcocos2dcpp.so:002a87d2` | 3, 4, 5 | ambiguous receiver class |

Three other calls in `002a7594` (3, 4, 7) were already independently
resolved by strict GameSceneUI/Controls target convergence; this probe must
**not** reuse their target across the remaining five calls without new
receiver-value evidence.

For the three ambiguous calls, v11 caller-type evidence lists both
`ControlsButton` and `cocos2d::CCTouch`. The catalog's `CCTouch` primary
prefix ends at `0x20`. **Neither caller argument class should be treated as
the virtual receiver without receiver-value provenance.**

## Independent exact-APK Thumb observations

The original `libcocos2dcpp.so` has SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.
ARM Thumb disassembly at ELF addresses (Ghidra image base is +`0x10000`)
shows the following field loads before the 17 unresolved slot-`0x80`
`BLX` calls. This is an **evidence hypothesis for Ghidra cross-check**,
not an accepted production receiver inference.

| Function (Ghidra) | Calls | Owner field offsets | Factory-backed candidate |
| --- | --- | --- | --- |
| `002a74c0` | 0, 1, 2 | `Controls +0x12c/+0x130/+0x134` | `ControlsButton` |
| `002a74c0` | 3 | `Controls +0x124` | `CCJoyStick` |
| `002a7594` | 0, 1, 2, 5, 6 | `Controls +0x12c/+0x130/+0x134` | `ControlsButton` |
| `002a7696` | 0 | `Controls +0x124` | `CCJoyStick` |
| `002a7696` | 1 | `Controls +0x174` | `cocos2d::CCSprite` |
| `002a78cc` | 0, 1, 2 | `Controls +0x12c/+0x130/+0x134` | `ControlsButton` |
| `002a87d2` | 3, 4, 5 | `ControlsButton +0x104/+0x10c/+0x108` | `cocos2d::CCSprite` |

The relevant factory return stores in `Controls::init()` are ELF
`0x298066` (`CCJoyStick::create` -> `+0x124`),
`0x2980e0` (`CCSprite::create` -> `+0x174`),
`0x298120`, `0x298172`, `0x2981c2`
(`ControlsButton::create` -> `+0x12c/+0x130/+0x134`).
`ControlsButton::initWithNormalImage()` stores `CCSprite::create`
returns at ELF `0x2984cc`, `0x2984ea`, `0x298510`
(`+0x104/+0x108/+0x10c`); the two-image overload also writes
`+0x104/+0x108` at `0x2985c4/0x2985de`.

The catalog's slot-`0x80` target for `ControlsButton` and `CCJoyStick`
is `libcocos2dcpp.so:00525d2e`, but for `cocos2d::CCSprite` it is
`libcocos2dcpp.so:00549f1e`. If field provenance holds for all callsites,
13 of the 17 calls would have the former target and four the latter.

**v11 provenance warning:** calls 3, 4, 7 of `002a7594` were previously
recovered by an audit of the `Controls::showControlsAttackKey()` *caller*
argument. That argument is not automatically the receiver of a nested virtual
call on `Controls +0x12c/+0x130/+0x134`. Their existing target happens to
match the `ControlsButton` slot-`0x80` target, but the reported
`GameSceneUI/Controls` receiver-class evidence should not be promoted
without a corrected field-level proof.

## Run (Manjaro, local Ghidra)

```bash
cd `/nevergone-recomp
git switch main
git pull --ff-only
bash tools/ml/run_nevergone_slot80_receiver_slice_probe.sh
```

The runner plans the exact current v11 failures, requires 17 calls across five
source functions, runs the existing read-only Ghidra structural receiver
exporter with 128 nodes / 20 depth, then audits each call's address, source,
slot, root, node count, truncation and candidate class-slot feasibility.
It refuses missing/stale/incomplete structural slices.

Output in `$HOME/.cache/nevergone-recomp/receiver-provenance/`:

- `slot80-receiver-slice-plan.tsv`
- `evidence/slot80-receiver-slices/*.tsv`
- `slot80-receiver-slice-audit.jsonl`
- `slot80-receiver-slice-audit.txt`

Package the audit and raw slices for review:

```bash
WORK="$HOME/.cache/nevergone-recomp/receiver-provenance"
tar -C "$WORK" -czf "$HOME/nevergone-slot80-receiver-evidence.tar.gz" \
  slot80-receiver-slice-plan.tsv \
  slot80-receiver-slice-audit.jsonl \
  slot80-receiver-slice-audit.txt \
  evidence/slot80-receiver-slices
```

The planner/auditor are evidence-only: no class inference is accepted and
`affects_routing` remains `false`. Synthetic smoke covers missing slices,
call-address tampering, inference/failure mismatch, and catalog prefix bounds.
