package org.nevergone.recomp;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.Arrays;

public final class GameSceneAtlasListDiscoverySmoke {
    private static void write(File file, String content) throws Exception {
        File parent = file.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IllegalStateException("mkdir failed: " + parent);
        }
        Files.write(file.toPath(), content.getBytes(StandardCharsets.UTF_8));
    }

    private static String plist(String gameData, String... atlases) {
        StringBuilder array = new StringBuilder();
        for (String atlas : atlases) array.append("<string>").append(atlas).append("</string>");
        return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" +
                "<plist version=\"1.0\"><dict>" +
                "<key>gs_num</key><integer>1</integer>" +
                "<key>gs01</key><dict>" +
                "<key>gamedatafile</key><string>" + gameData + "</string>" +
                "<key>eventID</key><integer>7</integer>" +
                "<key>gsresfile04</key><array>" + array + "</array>" +
                "</dict></dict></plist>";
    }

    public static void main(String[] args) throws Exception {
        File root = Files.createTempDirectory("nevergone-atlas-list-smoke").toFile();
        File assetRoot = new File(root, "assets");
        File gameScene = new File(assetRoot, "gamescene");
        if (!gameScene.mkdirs()) throw new IllegalStateException("mkdir failed");

        write(new File(gameScene, "unrelated.plist"),
                plist("some_other_scene.glData", "other/default.plist"));
        File hiddenName = new File(gameScene, "nested/not-guessed-name.plist");
        write(hiddenName, plist(
                "gamescene/gs_list/pvp_scene.glData",
                "l01/res/L01_01_default.plist",
                "l01/res/L01_01_effect.plist"));

        GameSceneAtlasListDiscovery.Result result =
                GameSceneAtlasListDiscovery.discover(assetRoot, "pvp_scene.glData");
        assert result != null;
        assert result.sceneListRelativePath.equals("gamescene/nested/not-guessed-name.plist");
        assert result.atlasPlists.equals(Arrays.asList(
                "l01/res/L01_01_default.plist",
                "l01/res/L01_01_effect.plist"));

        // Path prefixes in gamedatafile are ignored exactly at the basename
        // boundary, but a different target must not match.
        assert GameSceneAtlasListDiscovery.discover(assetRoot, "missing.glData") == null;

        // Ambiguous imported scene-list files fail closed instead of choosing by
        // filesystem order or filename.
        File duplicate = new File(gameScene, "duplicate.plist");
        write(duplicate, plist("pvp_scene.glData", "duplicate/default.plist"));
        assert GameSceneAtlasListDiscovery.discover(assetRoot, "pvp_scene.glData") == null;
        if (!duplicate.delete()) throw new IllegalStateException("delete failed");

        // The preload list may contain nested imported resource paths, but never
        // path traversal.
        write(hiddenName, plist("pvp_scene.glData", "../outside.plist"));
        assert GameSceneAtlasListDiscovery.discover(assetRoot, "pvp_scene.glData") == null;

        // Restore a valid file and prove an empty gsresfile04 array is a valid,
        // unambiguous recovered preload list rather than a missing match.
        write(hiddenName, plist("pvp_scene.glData"));
        result = GameSceneAtlasListDiscovery.discover(assetRoot, "pvp_scene.glData");
        assert result != null;
        assert result.atlasPlists.isEmpty();

        deleteTree(root);
    }

    private static void deleteTree(File file) {
        File[] children = file.listFiles();
        if (children != null) {
            for (File child : children) deleteTree(child);
        }
        if (!file.delete()) throw new IllegalStateException("delete failed: " + file);
    }
}
