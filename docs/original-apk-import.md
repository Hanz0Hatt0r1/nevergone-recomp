# Original APK asset import

The recompilation repository does not contain the proprietary Never Gone game assets. The Android shell can import them from a user-selected, legally obtained original APK at runtime.

## Flow

1. Tap **Import original Never Gone APK** in the recomp build.
2. Android's Storage Access Framework opens a system file picker.
3. The selected APK is treated as a ZIP archive.
4. Entries under `assets/` are imported; the legacy `assets/assets/` Lua tree is flattened into the same runtime resource root.
5. Files are first written to `<files>/assets.importing`, overlaying any existing imported OBB resources.
6. Paths are canonicalized and rejected if they escape the staging root.
7. A successful import replaces `<files>/assets`; the previous tree is kept as `<files>/assets.previous` until activation succeeds. If the process dies before activation, the next startup restores the backup.
8. Startup diagnostics are rerun immediately. The existing runtime then looks for `assets/Script/Game/StartLua.lua` and reports the next missing native bindings.

The app never deletes the user-selected APK or OBB. See [resource lifecycle](imported-game-resource-lifecycle.md) for staging, recovery, and the exact readiness checks.

The importer never adds original game data to Git and does not require legacy external-storage permissions.

## Compatibility

The implementation intentionally uses `java.io.File` rather than `java.nio.file.Files` so it remains compatible with the project's `minSdk 23` target.
