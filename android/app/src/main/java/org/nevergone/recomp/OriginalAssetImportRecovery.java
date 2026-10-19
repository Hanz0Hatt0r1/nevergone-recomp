package org.nevergone.recomp;

import java.io.File;
import java.io.IOException;

/**
 * Recovers the committed asset tree after a process dies between the two
 * directory renames used by the APK/OBB importers.
 *
 * A partially extracted staging tree is deliberately never activated.
 * The user-supplied APK/OBB archive is never touched or deleted.
 */
final class OriginalAssetImportRecovery {
    private OriginalAssetImportRecovery() {}

    static boolean recover(File filesDir) throws IOException {
        if (filesDir == null || !filesDir.isDirectory()) {
            throw new IOException("app-private files directory is unavailable");
        }

        File assets = new File(filesDir, "assets");
        File backup = new File(filesDir, "assets.previous");

        if (assets.exists()) {
            if (!assets.isDirectory()) {
                throw new IOException("imported assets path is not a directory");
            }
            // A previous activation succeeded. Never replace it with an older
            // backup just because cleanup was interrupted.
            return false;
        }
        if (!backup.exists()) return false;
        if (!backup.isDirectory()) {
            throw new IOException("previous assets backup is not a directory");
        }
        if (!backup.renameTo(assets)) {
            throw new IOException("cannot restore previous imported assets");
        }
        return true;
    }

    static boolean apkAssetsReady(File filesDir) {
        return existsNonEmpty(filesDir, "Script/Game/StartLua.lua") &&
                existsNonEmpty(filesDir, "config/share/GameChapterConfig.hpc");
    }

    static boolean obbAssetsReady(File filesDir) {
        return existsNonEmpty(
                    filesDir, "gamescene_ui/LevelUI/Gate_Background_UI/Gate_BackgroundPNG_01.plist") &&
                existsNonEmpty(
                    filesDir, "gamescene_ui/LevelUI/Gate_Background_UI/Gate_BackgroundPNG_01.png") &&
                existsNonEmpty(filesDir, "gamescene/gs_list/pvp_scene.glData");
    }

    private static boolean existsNonEmpty(File filesDir, String relative) {
        if (filesDir == null) return false;
        File asset = new File(new File(filesDir, "assets"), relative);
        return asset.isFile() && asset.length() > 0L;
    }
}
