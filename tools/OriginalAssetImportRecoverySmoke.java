package org.nevergone.recomp;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.file.Files;

/**
 * Standalone host smoke: javac -d /tmp/nevergone-asset-recovery
 *   android/app/src/main/java/org/nevergone/recomp/OriginalAssetImportRecovery.java
 *   tools/OriginalAssetImportRecoverySmoke.java
 * java -cp /tmp/nevergone-asset-recovery org.nevergone.recomp.OriginalAssetImportRecoverySmoke
 */
public final class OriginalAssetImportRecoverySmoke {
    private static void require(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    private static File nonEmpty(File root, String relative) throws IOException {
        File file = new File(root, relative);
        File parent = file.getParentFile();
        if (!parent.isDirectory() && !parent.mkdirs()) {
            throw new IOException("cannot create directory: " + parent);
        }
        try (FileOutputStream stream = new FileOutputStream(file)) {
            stream.write(0x37);
        }
        return file;
    }

    private static void deleteTree(File file) throws IOException {
        if (!file.exists()) return;
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children == null) throw new IOException("cannot list: " + file);
            for (File child : children) deleteTree(child);
        }
        if (!file.delete()) throw new IOException("cannot delete: " + file);
    }

    private static void testRecovery() throws IOException {
        File files = Files.createTempDirectory("nevergone-assets-recovery").toFile();
        try {
            File sourceObb = nonEmpty(files, "main.9.com.hippiegame.nevergone.obb");
            require(!OriginalAssetImportRecovery.recover(files), "empty install must not restore");
            require(!OriginalAssetImportRecovery.apkAssetsReady(files), "APK must be absent");
            require(!OriginalAssetImportRecovery.obbAssetsReady(files), "OBB must be absent");

            // Interrupted import: existing assets moved aside, staging incomplete.
            nonEmpty(files, "assets.previous/Script/Game/StartLua.lua");
            nonEmpty(files, "assets.previous/config/share/GameChapterConfig.hpc");
            nonEmpty(files, "assets.obb-importing/unverified.bin");
            require(OriginalAssetImportRecovery.recover(files), "backup must restore");
            require(OriginalAssetImportRecovery.apkAssetsReady(files), "restored APK resources");
            require(!OriginalAssetImportRecovery.obbAssetsReady(files), "incomplete OBB not ready");
            require(new File(files, "assets.obb-importing/unverified.bin").isFile(),
                    "recovery must not trust or promote staging");
            require(sourceObb.isFile() && sourceObb.length() == 1,
                    "source OBB must not be removed or modified");
            require(!OriginalAssetImportRecovery.recover(files), "restart must be idempotent");

            nonEmpty(files, "assets/gamescene_ui/LevelUI/Gate_Background_UI/Gate_BackgroundPNG_01.plist");
            nonEmpty(files, "assets/gamescene_ui/LevelUI/Gate_Background_UI/Gate_BackgroundPNG_01.png");
            nonEmpty(files, "assets/gamescene/gs_list/pvp_scene.glData");
            require(OriginalAssetImportRecovery.obbAssetsReady(files), "complete import ready");
            new File(files, "assets/gamescene/gs_list/pvp_scene.glData").delete();
            require(!OriginalAssetImportRecovery.obbAssetsReady(files),
                    "truncated import cannot be treated as ready");

            nonEmpty(files, "assets.previous/backup-only.dat");
            require(!OriginalAssetImportRecovery.recover(files), "current tree takes priority");
            require(new File(files, "assets.previous/backup-only.dat").isFile(),
                    "valid backup should not be silently discarded");
        } finally {
            deleteTree(files);
        }
    }

    private static void testInvalidBackups() throws IOException {
        File files = Files.createTempDirectory("nevergone-assets-invalid").toFile();
        try {
            nonEmpty(files, "assets.previous");
            boolean failed = false;
            try {
                OriginalAssetImportRecovery.recover(files);
            } catch (IOException expected) {
                failed = true;
            }
            require(failed, "non-directory backup must fail closed");
            deleteTree(new File(files, "assets.previous"));

            nonEmpty(files, "assets");
            failed = false;
            try {
                OriginalAssetImportRecovery.recover(files);
            } catch (IOException expected) {
                failed = true;
            }
            require(failed, "non-directory active tree must fail closed");
        } finally {
            deleteTree(files);
        }
    }

    public static void main(String[] args) throws Exception {
        testRecovery();
        testInvalidBackups();
    }
}
