package org.nevergone.recomp;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.Arrays;
import java.util.List;

public final class GameSceneAtlasResolverSmoke {
    private static void write(File file, String content) throws Exception {
        File parent = file.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IllegalStateException("mkdir failed: " + parent);
        }
        Files.write(file.toPath(), content.getBytes(StandardCharsets.UTF_8));
    }

    private static String plist(String directTextureFileName, boolean nestedTargetOnly) {
        String metadata;
        if (directTextureFileName != null) {
            metadata = "<key>metadata</key><dict><key>textureFileName</key><string>" +
                    directTextureFileName + "</string></dict>";
        } else if (nestedTargetOnly) {
            metadata = "<key>metadata</key><dict><key>target</key><dict>" +
                    "<key>textureFileName</key><string>ignored-nested-name</string>" +
                    "</dict></dict>";
        } else {
            metadata = "<key>metadata</key><dict></dict>";
        }
        return "<?xml version=\"1.0\"?><plist version=\"1.0\"><dict>" +
                "<key>frames</key><dict></dict>" + metadata + "</dict></plist>";
    }

    public static void main(String[] args) throws Exception {
        File root = Files.createTempDirectory("nevergone-atlas-resolver-smoke").toFile();
        File assets = new File(root, "assets");

        File explicitPlist = new File(assets, "gamescene/l01/res/explicit.plist");
        File explicitPng = new File(assets, "gamescene/l01/res/textures/explicit-atlas.png");
        write(explicitPlist, plist("textures/explicit-atlas.png", false));
        write(explicitPng, "png");

        File fallbackPlist = new File(assets, "gamescene/fallback.plist");
        File fallbackPng = new File(assets, "gamescene/fallback.png");
        write(fallbackPlist, plist(null, true));
        write(fallbackPng, "png");

        GameSceneAtlasListDiscovery.Result discovery = new GameSceneAtlasListDiscovery.Result(
                "gamescene/list.plist",
                Arrays.asList("l01/res/explicit.plist", "fallback.plist"));
        List<GameSceneAtlasResolver.Atlas> atlases = GameSceneAtlasResolver.resolve(assets, discovery);
        assert atlases != null;
        assert atlases.size() == 2;
        assert atlases.get(0).plistRelativePath.equals("gamescene/l01/res/explicit.plist");
        assert atlases.get(0).textureRelativePath.equals(
                "gamescene/l01/res/textures/explicit-atlas.png");
        assert atlases.get(1).plistRelativePath.equals("gamescene/fallback.plist");
        assert atlases.get(1).textureRelativePath.equals("gamescene/fallback.png");

        // A nested metadata/target textureFileName is intentionally ignored:
        // the recovered cocos2d-x loader asks metadata directly and falls back
        // to replacing the plist extension with .png when that key is empty.
        assert !atlases.get(1).textureRelativePath.contains("ignored-nested-name");

        File duplicateRoot = new File(assets, "duplicate.plist");
        File duplicateGameScene = new File(assets, "gamescene/duplicate.plist");
        write(duplicateRoot, plist(null, false));
        write(new File(assets, "duplicate.png"), "png");
        write(duplicateGameScene, plist(null, false));
        write(new File(assets, "gamescene/duplicate.png"), "png");
        GameSceneAtlasListDiscovery.Result ambiguous = new GameSceneAtlasListDiscovery.Result(
                "gamescene/list.plist", Arrays.asList("duplicate.plist"));
        assert GameSceneAtlasResolver.resolve(assets, ambiguous) == null;

        File traversalPlist = new File(assets, "gamescene/traversal.plist");
        write(traversalPlist, plist("../outside.png", false));
        write(new File(assets, "outside.png"), "png");
        GameSceneAtlasListDiscovery.Result traversal = new GameSceneAtlasListDiscovery.Result(
                "gamescene/list.plist", Arrays.asList("traversal.plist"));
        assert GameSceneAtlasResolver.resolve(assets, traversal) == null;

        deleteTree(root);
    }

    private static void deleteTree(File file) {
        File[] children = file.listFiles();
        if (children != null) for (File child : children) deleteTree(child);
        if (!file.delete()) throw new IllegalStateException("delete failed: " + file);
    }
}
