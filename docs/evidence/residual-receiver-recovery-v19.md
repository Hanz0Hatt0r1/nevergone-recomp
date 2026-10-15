# Residual receiver recovery v19

This stage consumes the real seven-call non-slot-`0x80` frontier produced by v18 and resolves only identities that can be tied to previously accepted receiver proofs through exact structural provenance.

## Accepted recoveries

v19 adds exactly five receiver queries:

| Source | Call | Slot | Receiver | Proof |
| --- | ---: | ---: | --- | --- |
| `libcocos2dcpp.so:002a08fc` | 0 | `0xd4` | `AndroidSdkMgr` | receiver `LOAD` uses the exact same structural address token as already recovered call 1, whose singleton cell `0x0094cbd8` is constructor/store/return proven |
| `libcocos2dcpp.so:002a647e` | 0 | `0xec` | `cocos2d::CCSprite` | `LOAD(parameter0 + 0x108)` in the same `CCJoyStick::setStickTexture` owner; `CCJoyStick::init` stores the exact same `CCSprite::create()` callee into `+0x108` as into already proven `+0x104` |
| `libcocos2dcpp.so:002a647e` | 2 | `0x54` | `cocos2d::CCSprite` | canonical receiver root is the exact same SSA `CALL` token as already recovered call 1 (`CCSprite::create`) |
| `libcocos2dcpp.so:002a647e` | 3 | `0xc0` | `cocos2d::CCSprite` | same exact `CCJoyStick+0x108` field/factory proof |
| `libcocos2dcpp.so:002a647e` | 5 | `0x3c` | `cocos2d::CCSprite` | same exact `CCJoyStick+0x108` field/factory proof |

Each target is then checked against `StaticVtableSlotCatalog/3`; the exact class/vtable/slot must have one static function-memory-pointer target and no runtime mismatch.

No neighboring-field class inference, target-name inference, cross-vtable transfer, or production routing change is allowed.

## Real evidence result

The user-captured v18/v19 evidence accepted all five planned recoveries. The v17 frontier of seven unresolved receiver classes is therefore reduced to exactly two:

- `libcocos2dcpp.so:002a47c6` call 6, slot `0x180`: indexed `BeatenEffect + (phi + 0x14c)` receiver with a real SSA `MULTIEQUAL` loop;
- `libcocos2dcpp.so:002a5dd0` call 0, slot `0x14c`: `LOAD(this + 0x14c)` receiver whose original v18 canonical-target exporter failed with `target_address_not_constant_form`.

Those two remain unresolved and are not modified by v19.

## Run

After v17 and v18 evidence exist under `$HOME/.cache/nevergone-recomp/receiver-provenance`:

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only

bash tools/ml/run_nevergone_residual_receiver_recovery_v19.sh
```

The runner re-exports exact `CCJoyStick::init` field stores, applies the fail-closed finalizer, resolves the updated queries through the static vtable catalog, audits receiver failures, and requires an exact query delta of `+5` with exactly the two residual calls above.

Output archive:

```text
~/nevergone-residual-receiver-recovery-v19.tar.gz
```

The stage remains evidence-only (`affects_routing=false`).
