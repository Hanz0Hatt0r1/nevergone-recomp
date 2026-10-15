# PLAYERFACEDIR call-site evidence

The project already proves the horizontal control/facing path:

```text
CONTROLSSTATE 0x12 -> MEPlayer::filpPlayer(true, ...)
CONTROLSSTATE 0x13 -> MEPlayer::filpPlayer(false, ...)
```

and `filpPlayer(bool, ...)` retains that bool at `MEPlayer +0x12c` and forwards
it to `CCSprite::setFlipX(bool)`. This is enough for the tutorial
`player_is_face` input, but it does not prove the integer `PLAYERFACEDIR` value
forwarded separately to enemy creation.

## Probe

Run against the existing analyzed original 1.0.9 Ghidra project:

```bash
python3 tools/run_player_face_dir_call_probe.py \
  --ghidra /opt/ghidra \
  --project-dir /path/to/ghidra/projects \
  --project-name Nevergone \
  --program libcocos2dcpp.so
```

The known analyzed-image defaults are:

```text
MEPlayer::setControlsState(CONTROLSSTATE)  0032ddee
MEPlayer::setPlayerDir(PLAYERFACEDIR)      0032c75a
```

The wrapper exports metadata-only high P-code for `setControlsState()` and then
reports every direct `setPlayerDir()` call plus the structural source of its
second data argument. `ExportFunctionPcodeFlow` omits the call target itself,
so data input 0 is the C++ `this` value and data input 1 is `PLAYERFACEDIR`.

Output:

```text
build/player-face-dir-call-evidence/set-controls-state-pcode.tsv
build/player-face-dir-call-evidence/player-face-dir-call-report.txt
```

## Source classifications

The analyzer follows only bounded value-preserving operations and reports:

- `constant` — one exact integer reaches the argument;
- `constant-set` — a `MULTIEQUAL`/phi resolves only to a finite set of exact
  constants;
- `root` — the argument is a direct unresolved input/register token;
- `derived` — arithmetic or another transformation remains between the source
  and the argument;
- `missing-argument` — the exported direct call does not expose a second data
  argument and is therefore unusable evidence.

The report aggregates observed constant candidates but deliberately does not
assign them to `CONTROLSSTATE 0x12` or `0x13`. Call-site addresses can be
reviewed against the already recovered `setControlsState()` branch anchors near
ELF `0x0031de36`, `0x0031de50`, `0x0031df3e`, and `0x0031df4c`.

If the direct call arguments resolve to constants, that review should be enough
to establish the dynamic `PLAYERFACEDIR` contract. If a call remains `derived`,
the report exposes the exact call site and root tokens for the next bounded
slice instead of substituting a likely enum value.
