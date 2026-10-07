# Lua module dependency map

The recovered asset decoder makes it possible to analyze the original Lua sources locally without committing them. `tools/lua_dependency_map.py` builds a metadata-only static graph from string-literal loader calls.

The graph is based on these call forms found in the shipped scripts:

```text
require(...)
CAddDoString(...)
dofile(...)
```

It intentionally does not attempt to publish, reproduce, or decompile script bodies.

## Known APK baseline

Running the tool directly against the known original APK produces:

| Metric | Result |
| --- | ---: |
| Lua modules | 107 |
| Static loader references | 108 |
| Resolved references | 104 |
| Unresolved references | 4 |
| Unique resolved edges | 104 |
| Modules reachable from `Game.StartLua` | 101 |
| Modules outside the static `StartLua` graph | 6 |

The decoded text encodings are also consistent with the decoder validation:

- 99 modules: UTF-8/UTF-8-with-BOM compatible;
- 8 modules: GB18030-compatible because of legacy Simplified-Chinese text/comments.

## Startup fan-out

`Game.StartLua` has only two direct static module dependencies:

```text
Game.ClientRequire
ShareLogic.require
```

Those two files are the principal aggregators for the client script environment:

| Module | Direct loader references |
| --- | ---: |
| `Game.ClientRequire` | 50 |
| `ShareLogic.require` | 48 |
| `Game.Logic.HpGame` | 8 |
| `Game.StartLua` | 2 |

The 101 modules reachable from `Game.StartLua` consist of 52 `Game.*` modules and 49 `ShareLogic.*` modules. The largest reachable groups include `ShareLogic.Logic`, `Game.Logic`, `ShareLogic.Public`, `Game.Public`, and `Game.Engine`.

This is useful for recompilation because it shows that almost the entire client-side Lua layer is loaded through a small, deterministic startup fan-out rather than by an opaque runtime discovery mechanism.

## Unresolved references

Four static references cannot be resolved to any Lua file in the APK. All originate from `Game.Logic.HpGame` through `CAddDoString`:

```text
share.share_gameLogic_require
share.share_public_require
system.s_require
user.require
```

These names look like shared/server-side module roots rather than client APK paths. More importantly, `Game.Logic.HpGame` itself is not reachable from the normal `Game.StartLua` static graph, so the missing modules are not currently evidence of a blocker for ordinary client startup.

They should still be retained in the metadata because they may represent development tooling, server simulation code, an alternate runtime mode, or content expected from a different product build.

## Modules outside the normal static startup graph

Six shipped modules are not reachable from `Game.StartLua` through the currently recognized literal loader calls:

```text
Game.Engine.CocosInterface
Game.Logic.HPFrined
Game.Logic.HPMail
Game.Logic.HPSendAuction
Game.Logic.HPTeam
Game.Logic.HpGame
```

`HPFrined` is recorded with the spelling present in the shipped asset name.

Four of these (`HPFrined`, `HPMail`, `HPSendAuction`, `HPTeam`) are referenced by `Game.Logic.HpGame`, so they form a separate small subgraph. `Game.Engine.CocosInterface` currently has no resolved incoming loader edge from the normal startup graph and may be loaded through native code, a dynamically constructed module name, or code that is not represented by the simple literal-call scan.

## Resolution rules

The tool derives module names from paths below `Script/`, then resolves requests in this order:

1. normalize slashes to dots;
2. remove a trailing `.lua`;
3. remove a leading `Script.`;
4. exact module-name match;
5. unique suffix match.

The suffix rule is needed for calls such as a shorter `Engine.*` name resolving uniquely to the corresponding `Game.Engine.*` module.

Only unambiguous matches are accepted. An ambiguous or absent target remains unresolved rather than being guessed.

## Reproducing the graph

Run directly against a legally obtained original APK:

```bash
python3 tools/lua_dependency_map.py /path/to/com.hippiegame.nevergone.apk \
  --json build/lua-dependencies.json \
  --markdown build/lua-dependencies.md
```

The original APK input is decoded entirely in memory through the recovered asset transform. No decoded script files are written by this tool.

It can also analyze a local already-decoded tree:

```bash
python3 tools/lua_dependency_map.py build/decoded-assets/assets/Script \
  --entry Game.StartLua \
  --markdown build/lua-dependencies.md
```

The JSON output contains module names, source paths, encoding classification, reachability, dependency edge metadata, and unresolved module names. It does not contain script source text.

## Limitations

This is a static index, not a full Lua parser or runtime tracer. It will not discover:

- module names assembled dynamically at runtime;
- loaders hidden behind functions other than the currently recognized calls;
- native C/C++ code that loads a Lua module directly;
- dependencies selected only after runtime conditions;
- reflective/global lookups that do not load a module.

The graph should therefore be treated as a high-confidence lower bound for the startup dependency set rather than proof that no other runtime loading occurs.

## Recompilation implications

The recovered graph changes the immediate implementation priorities:

1. Native reconstruction no longer needs to guess the entire game initialization order before testing Lua.
2. A modern runtime can load `Game.StartLua` and expect the two aggregator modules to establish nearly the full client Lua environment.
3. Native binding discovery can be prioritized by scanning the reachable 101-module set for global/API identifiers and matching those to C++ registration code.
4. The isolated `HpGame` subgraph and four absent server/shared roots can be deferred until evidence shows they are needed for offline client gameplay.
5. `Game.Engine.CocosInterface` should be investigated specifically because it is shipped but not statically reached; it is a strong candidate for a native-driven or dynamically named bridge.

The next reverse-engineering step is to map the Lua-to-C++ API surface: identify functions/classes registered into Lua by the native executable, then determine which of those bindings are actually used by the reachable client graph.
