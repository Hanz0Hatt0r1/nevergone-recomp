# P1A: ControlsButton sprite receiver fields v17

v17 closes the three remaining ambiguous slot-`0x80` receiver rows in
`ControlsButton::touchMoved` (`002a87d2`). These rows were deliberately left
unpromoted by v15/v16 because the construction-linked `CCSprite` factory
witnesses did not, by themselves, exclude later owner-field replacement.

| Source/call | Actual receiver field | Factory class | Slot `0x80` target |
| --- | --- | --- | --- |
| `002a87d2` call 3 | `ControlsButton+0x104` | `cocos2d::CCSprite` | `00549f1e` |
| `002a87d2` call 4 | `ControlsButton+0x10c` | `cocos2d::CCSprite` | `00549f1e` |
| `002a87d2` call 5 | `ControlsButton+0x108` | `cocos2d::CCSprite` | `00549f1e` |

## Evidence joined in v17

The existing slot-`0x80` factory-field probe already proves, for each selected
call, that the nested CALLIND receiver is a structural load from the exact
`ControlsButton` owner field and that the field's initializer is
`cocos2d::CCSprite::create`.

The exact factory STORE witnesses are:

- `+0x104`: `0x2984cc` and `0x2985c4`;
- `+0x108`: `0x2984ea` and `0x2985de`;
- `+0x10c`: `0x298510`.

That evidence remains intentionally marked
`field_writes_exhaustive=false`, `receiver_class_proven=false`, and
`affects_routing=false`.

PR #605 added the missing independent owner-side proof. The
`NEVERGONE.ControlsButtonOwnerFieldPackagedStability/1` artifact closes the
exact packaged normal-code alias surface for `+0x104/+0x108/+0x10c`:

- all eleven direct target-field writes remain the pinned initializer/factory
  writes;
- the seven `this+0x120` CCPoint/CCRect subobject calls cannot recover the
  owner;
- the targeted-touch retained delegate path through `ControlsButton+0xe0`
  returns only to the already-inventoried callback methods and does not write
  the sprite fields;
- child `CCNode+0xa8` parent back-references are bounded by the complete parent
  access inventory and inherited parent slots;
- inherited `CCLayer` lifecycle paths do not write the sprite fields;
- packaged script-engine write-through and packaged `dlsym` setter lookup are
  excluded.

The owner stability artifact still has `receiver_class_proven=false`. v17 does
not reinterpret that flag. Instead it joins the global field-stability proof
with each exact per-call receiver slice and factory witness. Only that joined
proof is allowed to promote a call.

## v17 finalizer

`finalize_nevergone_controlsbutton_sprite_field_recovery_v17.py` starts from
v16 and requires all three selected rows to remain
`ambiguous_receiver_class`. For every row it verifies:

- exact source/call identity (`002a87d2`, calls 3/4/5);
- inference and candidate call-address equality;
- slot `0x80`;
- exact `ControlsButton` owner-field identity;
- exact `CCSprite` factory witness STORE set;
- candidate class `cocos2d::CCSprite`;
- vtable `libcocos2dcpp.so:009363d8`;
- unique slot target `libcocos2dcpp.so:00549f1e`;
- no runtime mismatch and no cross-class transfer;
- the complete packaged owner-field closure described above.

A valid baseline adds exactly **three** exact-vtable queries and changes only
these three inference rows to `resolved_unique_vtable`. The previous
`ControlsButton`/`cocos2d::CCTouch` caller-type ambiguity is not used as
positive receiver evidence.

The proof scope remains `exact_packaged_apk_normal_code_execution`.
`whole_program_alias_writes_excluded=false` and
`external_injected_code_excluded=false` remain explicit.

## Run

The runner needs the same v16 inputs plus the 19-method ControlsButton owner
field evidence archive used by #605:

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only

bash tools/ml/run_nevergone_controlsbutton_sprite_field_recovery_v17.sh \
  /path/to/com.hippiegame.nevergone.apk \
  ~/nevergone-controls-field-alias-evidence.tar.gz \
  ~/nevergone-controls-alias-operand-roots.tar.gz \
  ~/nevergone-controlsbutton-owner-field-evidence.tar.gz
```

The runner regenerates v16, reruns the exact packaged ControlsButton owner
stability proof, finalizes v17, resolves the expanded query set, reruns the
receiver failure audit, and verifies that every non-target v16 inference row is
byte-for-byte unchanged.

Output:

`~/nevergone-controlsbutton-sprite-field-recovery-v17.tar.gz`

No APK/ELF bytes or decompiler C text are included in the archive.
