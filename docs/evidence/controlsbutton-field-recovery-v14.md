# P1A: ControlsButton field recovery v14

## Scope

v14 is the first recovery stage allowed to consume the complete **packaged
program** alias closure for the three quarantined calls in
`Controls::showControlsAttackKey(int)`:

| Call | CALLIND | Actual receiver field | Factory STORE |
| ---: | --- | --- | --- |
| 3 | `002a7612` | `Controls+0x134` | `002a81c2` |
| 4 | `002a7634` | `Controls+0x130` | `002a8172` |
| 7 | `002a7686` | `Controls+0x12c` | `002a8120` |

The old GameSceneUI/Controls **caller-argument** convergence is still rejected.
Each v14 row must carry the original quarantine reason
`caller_argument_not_virtual_receiver_provenance`; recovery uses the actual
nested CALLIND receiver only.

The scope is deliberately named
`exact_packaged_apk_normal_code_execution`. It excludes mutation by injected
code, debugger writes, memory corruption, hooks or a modified APK. Therefore
v14 does **not** set `whole_program_alias_writes_excluded=true`. The narrower
claim is that the exact packaged application has no discovered normal-code
path that replaces `Controls+0x12c/+0x130/+0x134` after their proven
`ControlsButton::create` initialization and before the relevant calls.

## Evidence joined by the packaged stability audit

`audit_nevergone_controlsbutton_packaged_field_stability.py` reruns or joins
all current exact evidence against the same original ARMv7 ELF SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`:

1. **Actual receiver value** — the Ghidra structural receiver slice for all
   eight calls in `002a7594`, with calls 3/4/7 resolving to field loads
   `+0x134/+0x130/+0x12c` and no caller-argument substitution.
2. **Factory/vtable provenance** — each field is initialized from
   `ControlsButton::create`; the constructor installs
   `libcocos2dcpp.so:008f46f8`, whose slot `0x80` is
   `libcocos2dcpp.so:00525d2e` (`cocos2d::CCNode::setVisible(bool)`).
3. **Complete 38-method local STORE inventory** — 167 STOREs and 293 calls;
   exactly the three factory stores directly write the target fields. The
   former 46 unknown STORE bases are all classified by exact original Thumb:
   37 direct ordinary `Controls` fields and nine child-object writes.
4. **All 78 Controls-derived call arguments** — exact native offsets split
   into 35 unadjusted `Controls*` calls and 43 embedded `CCPoint` subobjects;
   no offset remains unresolved.
5. **Retained callbacks/delegates** — `ControlsButton+0x110` callback object
   and `CCJoyStick+0x14c` delegate back-reference are closed to exact callback
   bodies that do not write the target fields.
6. **Scheduler/action callbacks** — `schedule`, `scheduleOnce` and
   `CCCallFuncN` retained `Controls*` callbacks are pinned and do not write the
   target fields.
7. **Cocos back-references** — parent and touch-handler retention are
   characterized; parent-native write-through is bounded and the packaged
   script-engine setter is statically unreachable.
8. **Dynamic symbol surface** — every packaged `dlsym` path and the second
   native library are pinned; no packaged lookup of
   `CCScriptEngineManager::setScriptEngine` exists.
9. **Remaining direct Cocos full-this calls** — exact original bodies for
   `CCLayer` construction/destruction and `CCNode::unscheduleAllSelectors`
   are hash-pinned and have no STORE to the target offsets. The three
   scheduling calls are covered by the scheduler callback closure.
10. **Controls live primary vptr** — the 38-method Ghidra STORE inventory has
    exactly two `this+0` vptr stores: constructor `002a698a` and destructor
    `002a6706`. Exact constructor/destructor bodies are pinned. The three
    relevant live-object indirect calls therefore cannot silently route
    through an unobserved packaged `Controls` vptr replacement.

Only after all gates succeed does the stability report emit
`packaged_controlsbutton_field_stability_proven=true` and three
`packaged_receiver_class_proven=true` detail rows. It simultaneously retains:

- `whole_program_alias_writes_excluded=false`;
- `external_injected_code_excluded=false`;
- `affects_routing=false` on the evidence artifact.

## v14 finalizer

`finalize_nevergone_controlsbutton_field_recovery_v14.py` starts from the
current v13 baseline. It accepts **only** calls 3, 4 and 7 and requires:

- v13 status is still `receiver_not_single_pointer`;
- exact call address and slot `0x80`;
- the old caller-argument recovery is still explicitly quarantined;
- exact receiver field identity from the packaged stability proof;
- class `ControlsButton` and vtable `libcocos2dcpp.so:008f46f8`;
- exactly one `StaticVtableSlotCatalog/3` row for that class/vtable/slot;
- single catalog target `libcocos2dcpp.so:00525d2e`;
- no runtime mismatch.

On success it emits three ordinary exact-vtable queries. Relative to a valid
v13 baseline the query count increases by **exactly three**. The expected
`receiver_not_single_pointer` count becomes zero because v13's only remaining
three such rows are these quarantined calls.

## Reproduce on Manjaro

The runner requires the two previously returned evidence archives plus the
exact original APK:

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only

bash tools/ml/run_nevergone_controlsbutton_field_recovery_v14.sh \
  /path/to/com.hippiegame.nevergone.apk \
  ~/nevergone-controls-field-alias-evidence.tar.gz \
  ~/nevergone-controls-alias-operand-roots.tar.gz
```

The runner regenerates v13, regenerates the actual mixed receiver slice with
read-only Ghidra, recomputes the packaged stability proof, finalizes v14,
resolves the resulting queries and reruns the receiver failure audit.

Output archive:

`~/nevergone-controlsbutton-field-recovery-v14.tar.gz`

No APK/ELF bytes or decompiler C text are included in the archive.
