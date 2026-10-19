# Imported game resource lifecycle

The reconstructed Android application retains user-owned APK and OBB files.
It does **not** emulate the original game's reported removal of its OBB after
first-run extraction. Import uses the Android Storage Access Framework and does
not delete, rename or modify the selected source archive.

## Evidence and scope

- Original-game Frida observations show successful `getFileData()` reads from
  `/data/user/0/com.hippiegame.nevergone/files/assets/`.
- Original startup artwork and some configuration files also exist in the base
  APK and may be read from the APK without first being copied.
- The original first-run OBB extraction and deletion sequence was reported by
  the user; the current traces establish later resource reads, not the exact
  extraction/deletion implementation.

The clean-room recomp intentionally keeps its explicit, user-initiated APK/OBB
import flow rather than silently importing or deleting another application's
files.

## Current runtime behavior

1. Import original APK resources to the application-private `files/assets`.
2. Import original OBB resources into the same tree. The OBB importer scans
   for the resource prefix instead of assuming `assets/` at ZIP root.
3. Each import builds a staging tree and validates required resources before
   switching the active directory. The OBB importer overlays the existing APK
   assets; explicit APK reimport now likewise preserves already imported OBB
   resources.
4. On startup, if a previous import was interrupted *after* moving
   `files/assets` to `files/assets.previous` but *before* activation,
   `OriginalAssetImportRecovery` restores that backup. It never promotes the
   potentially incomplete `assets.importing` or `assets.obb-importing` tree.
5. The runtime uses the existing `files/assets` tree on subsequent launches.
   No automatic repeated OBB import and no OBB deletion occur.

## Readiness contract

The APK is ready only when nonempty
`Script/Game/StartLua.lua` and
`config/share/GameChapterConfig.hpc` exist.

The expansion assets are ready only when these three files exist and are
nonempty:

- `gamescene/gs_list/pvp_scene.glData`
- `gamescene_ui/LevelUI/Gate_Background_UI/Gate_BackgroundPNG_01.plist`
- `gamescene_ui/LevelUI/Gate_Background_UI/Gate_BackgroundPNG_01.png`

These are intentionally **minimal readiness checks**, not a proof that every
tutorial/player/enemy asset needed for the full playable route is present.
Remaining missing assets are diagnosed by their actual runtime consumers.

## Regression coverage

`tools/OriginalAssetImportRecoverySmoke.java` exercises:

- restoration after an interrupted asset activation;
- idempotent subsequent launch;
- refusal to promote incomplete staging;
- preservation of an existing active tree over an older backup;
- rejection of invalid backup/active paths;
- nonempty APK/OBB sentinel checks;
- preservation of the user-owned OBB file.

Run the host smoke locally with:

```bash
mkdir -p /tmp/nevergone-asset-recovery
javac -d /tmp/nevergone-asset-recovery \
  android/app/src/main/java/org/nevergone/recomp/OriginalAssetImportRecovery.java \
  tools/OriginalAssetImportRecoverySmoke.java
java -cp /tmp/nevergone-asset-recovery \
  org.nevergone.recomp.OriginalAssetImportRecoverySmoke
```

This is also part of the fast GitHub CI job. On-device import, low-storage,
and process-termination scenarios still require separate real-device tests.
