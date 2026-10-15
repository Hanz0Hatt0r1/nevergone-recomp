# ControlsButton owner-field first-hop closure

This evidence-only stage continues the 19-method `ControlsButton` owner-field
probe for the remaining sprite receiver fields `+0x104/+0x108/+0x10c`.
It consumes the exact original APK/ELF and
`nevergone-controlsbutton-owner-field-evidence.tar.gz`.

## Exact packaged findings

The previous Ghidra inventory leaves one opaque local STORE. Exact ARMv7 bytes
identify `ControlsButton::setEnabled` at `002a865e` as a byte STORE to
`ControlsButton+0x11c`; it does not overlap the three sprite fields.

The 32 parameter-zero call edges split into 23 full-owner (`+0`) edges,
7 `+0x120` subobject edges and 2 `+0xe0` delegate-subobject edges. The 23 full
owner edges contain 8 calls to another already-inventoried `ControlsButton`
method and 15 external calls.

The 15 external full-owner sites resolve exactly to:

- 5 direct calls: `CCLayer` constructor, destructor, `onEnter`, `onExit`, and
  terminal `operator delete`;
- 5 virtual calls at slot `0xb8` -> `CCNode::addChild(CCNode*)`;
- 3 virtual calls at slot `0x84` -> `CCNode::isVisible()`;
- 2 virtual calls at slot `0x100` -> `CCNode::getTag()`.

The read-only accessors have no writes. The exact `addChild` owner-side path
mutates generic `CCNode` state at `+0xa4/+0xd2`, not the sprite fields.
Therefore this first hop contains no direct writer for
`ControlsButton+0x104/+0x108/+0x10c`.

## Deliberate residual frontier

This stage does **not** yet claim packaged owner-field stability. It preserves
three explicit alias classes for a later closure:

1. `ControlsButton+0xe0` targeted-touch delegate retention;
2. child `CCNode+0xa8` parent back-references established by `addChild`;
3. nested/retained `CCLayer` lifecycle paths.

Accordingly the audit emits `owner_fields_stable=false`,
`receiver_class_proven=false`, and `affects_routing=false`.

## Run

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only

bash tools/ml/run_nevergone_controlsbutton_owner_first_hop.sh \
  /path/to/com.hippiegame.nevergone.apk \
  ~/nevergone-controlsbutton-owner-field-evidence.tar.gz
```

Output: `~/nevergone-controlsbutton-owner-first-hop.tar.gz`.
