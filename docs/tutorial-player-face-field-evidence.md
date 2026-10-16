# Tutorial player face field evidence

The tutorial spawn plan consumes two separate MEPlayer-facing values:

- `MEPlayer::isFace()`;
- the current `PLAYERFACEDIR` field forwarded into enemy creation.

The first value is already linked to the recovered horizontal-control path:
`MEPlayer::filpPlayer(bool, ...)` retains the bool at `MEPlayer +0x12c`, and the
project-owned control runtime retains the same requested value as `flip_x`.

`PLAYERFACEDIR` is kept separate until its storage field is structurally
confirmed.

The probe prefers revenginstr's generic parameter-store exporter when its
sibling checkout is available. The local adapter changes only the generic format
cell to the established Nevergone format; the existing Nevergone exporter
remains the fallback for older shared checkouts.

## Probe

Run against the analyzed original 1.0.9 `libcocos2dcpp.so` in the existing
Ghidra project:

```bash
python3 tools/run_tutorial_player_face_field_probe.py \
  --ghidra /opt/ghidra \
  --project-dir /path/to/ghidra/projects \
  --project-name Nevergone \
  --program libcocos2dcpp.so
```

The default Ghidra entries (`0032c748` for `MEPlayer::isFace` and `0032c75a`
for `MEPlayer::setPlayerDir`) match the analyzed image used by the existing
facing evidence. Both can be overridden on the command line.

The wrapper creates:

```text
build/tutorial-player-face-fields/is-face-field.tsv
build/tutorial-player-face-fields/player-face-dir-field.tsv
build/tutorial-player-face-fields/tutorial-player-face-field-report.txt
```

`ExportAccessorReturnField.java` accepts only a return value loaded from a
parameter plus a constant field offset. `ExportParameterStoreField.java`
accepts only a value that traces through value-preserving operations from a
function parameter into another parameter plus a constant offset. The analyzer
then requires one unique `this` field in each path.

The report deliberately calls both offsets *field candidates*. It does not
assume that the two fields are identical, and it does not invent enum values for
`PLAYERFACEDIR`.

## Runtime consequence

Once `setPlayerDir()` storage and its value-producing callers are established,
the tutorial runtime can carry a real `PLAYERFACEDIR` alongside the already
recovered `isFace`/`flip_x` value. The remaining spawn-plan dependency will then
be the three raw scene-limit values used by `LessonLayer`.
