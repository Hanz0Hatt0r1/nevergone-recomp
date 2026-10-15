# ControlsButton packaged owner-field stability

This evidence-only stage closes the packaged alias surface for the final three
`ControlsButton` sprite fields: `+0x104`, `+0x108`, and `+0x10c`.

It consumes the exact original APK plus the 19-method owner-field evidence from
#596 and the first-hop closure from #602. It does not infer a receiver class or
add receiver queries.

## Joined proof

The gate requires all eleven known target-field STOREs to remain exactly the
pinned constructor/factory writes. The only formerly opaque local STORE is
`ControlsButton::setEnabled -> +0x11c`, outside the target fields.

The non-zero `ControlsButton`-relative call surface is also closed explicitly:

- all seven `this+0x120` edges are exact `CCPoint` construction/assignment or
  `CCRect::containsPoint` calls and cannot recover the owner object;
- the two `this+0xe0` edges are the targeted-touch delegate registration and
  removal paths. The delegate is retained at `CCTouchHandler+0x14`; the exact
  secondary `ControlsButton` touch vtable dispatches back to the known touch
  callbacks already included in the 19-method owner inventory;
- child `CCNode+0xa8` parent back-references are bounded by the complete parent
  access inventory and exact inherited `getParent/removeChild/reorderChild/
  nodeToParentTransform` slots. Their direct STORE inventories do not touch the
  sprite fields;
- inherited `CCLayer` construction/destruction/enter/exit bodies are SHA-pinned
  and have no direct STORE to the sprite fields. The owner inventory also shows
  no `ControlsButton` writes enabling the inherited script/touch activation
  flags beyond the explicit targeted-touch path.

The gate additionally reruns the packaged script-engine and dlsym exclusions
used by the established Controls proof. No packaged route to
`CCScriptEngineManager::setScriptEngine` may appear.

## Boundary

The result is `owner_fields_stable=true` only for
`exact_packaged_apk_normal_code_execution`. It deliberately keeps
`receiver_class_proven=false`, `whole_program_alias_writes_excluded=false`,
`external_injected_code_excluded=false`, and `affects_routing=false`.

## Run

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only

bash tools/ml/run_nevergone_controlsbutton_owner_packaged_stability.sh \
  /path/to/com.hippiegame.nevergone.apk \
  ~/nevergone-controlsbutton-owner-field-evidence.tar.gz
```

Output: `~/nevergone-controlsbutton-owner-packaged-stability.tar.gz`.
