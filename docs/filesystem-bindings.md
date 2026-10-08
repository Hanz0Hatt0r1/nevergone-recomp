# Reconstructed Lua filesystem bindings

The first post-startup compatibility set implements four Lua-visible filesystem helpers whose call shapes and roles are recoverable from the shipped scripts and native metadata.

| Lua API | Reconstructed contract |
| --- | --- |
| `Lua_GetSetFilePath()` | returns the Android app-private writable files directory with a trailing slash |
| `Lua_GetPathWithFileName(name)` | resolves a relative resource name against `<files>/assets` and the recovered ordered search directories |
| `LGG_IsFileExist(path)` | returns whether `path` names a regular file |
| `Lua_CopyFile(source, destination)` | creates destination parent directories as needed and overwrites the destination file |

## Resource resolution

The resolver preserves the search root reconstructed from `AppDelegate::AddAllSearchPath()`:

```text
<Android app files directory>/assets
```

It checks the root first and then the 59 recovered child search directories in original order. That order is important because duplicate filenames may rely on first-match behavior.

Relative input is sandboxed: absolute paths and parent traversal are rejected by the resource-name resolver. APIs that explicitly receive already-expanded writable paths (`LGG_IsFileExist`, `Lua_CopyFile`) operate on the supplied path because the recovered Lua layer constructs those paths from `Lua_GetSetFilePath()`.

## Error behavior

`Lua_CopyFile` is used as a command by the recovered startup-reachable Lua code rather than as a boolean expression. The compatibility wrapper therefore returns no Lua values on success and raises a Lua error on failure. This avoids silently accepting missing/corrupt local configuration data.

## Why these bindings are implemented now

They are used by startup-reachable modules such as the file/config management layer, and their semantics can be reconstructed without inventing obsolete network or platform-service behavior. They are registered through the central `native_binding_registry` so the missing-global diagnostics continue to expose the next unimplemented API.
