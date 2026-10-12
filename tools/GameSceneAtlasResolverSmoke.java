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

        List<String> orderedNames = Arrays.asList("l01/res/explicit.plist", "fallback.plist");
        List<GameSceneAtlasResolver.Atlas> directAtlases =
                GameSceneAtlasResolver.resolve(assets, orderedNames);
        assert directAtlases != null;
        assert directAtlases.size() == 2;
        assert directAtlases.get(0).plistRelativePath.equals("gamescene/l01/res/explicit.plist");
        assert directAtlases.get(0).textureRelativePath.equals(
                "gamescene/l01/res/textures/explicit-atlas.png");
        assert directAtlases.get(1).plistRelativePath.equals("gamescene/fallback.plist");
        assert directAtlases.get(1).textureRelativePath.equals("gamescene/fallback.png");

        // The legacy scene-list wrapper resolves the same ordered names. The
        // first-playable PVP route now supplies ordered names from LoadGL_Global
        // directly instead of rediscovering a scene-list plist.
        GameSceneAtlasListDiscovery.Result discovery = new GameSceneAtlasListDiscovery.Result(
                "gamescene/list.plist", orderedNames, 123.5f, -42.25f);
        List<GameSceneAtlasResolver.Atlas> atlases = GameSceneAtlasResolver.resolve(assets, discovery);
        assert atlases != null;
        assert atlases.size() == directAtlases.size();
        for (int i = 0; i < atlases.size(); ++i) {
            assert atlases.get(i).plistRelativePath.equals(directAtlases.get(i).plistRelativePath);
            assert atlases.get(i).textureRelativePath.equals(directAtlases.get(i).textureRelativePath);
        }

        // Resolver behavior remains driven only by the ordered atlas list; the
        // retained player-start metadata is evidence for a later player boundary.
        assert Float.compare(discovery.playerStartXPos, 123.5f) == 0;
        assert Float.compare(discovery.playerStartYPos, -42.25f) == 0;

        // A nested metadata/target textureFileName is intentionally ignored:
        // the recovered cocos2d-x loader asks metadata directly and falls back
        // to replacing the plist extension with .png when that key is empty.
        assert !atlases.get(1).textureRelativePath.contains("ignored-nested-name");

        // The reconstructed lookup now follows the complete AddAllSearchPath
        // order, not only GameScene directories. A basename in `other` is a
        // valid first match because that directory precedes the GameScene roots.
        File otherPlist = new File(assets, "other/global.plist");
        write(otherPlist, plist(null, false));
        write(new File(assets, "other/global.png"), "png");
        List<GameSceneAtlasResolver.Atlas> other =
                GameSceneAtlasResolver.resolve(assets, Arrays.asList("global.plist"));
        assert other != null && other.size() == 1;
        assert other.get(0).plistRelativePath.equals("other/global.plist");
        assert other.get(0).textureRelativePath.equals("other/global.png");

        // The writable assets root itself is the first Cocos search path. A
        // later duplicate in gamescene must not make the lookup ambiguous.
        File duplicateRoot = new File(assets, "duplicate.plist");
        File duplicateGameScene = new File(assets, "gamescene/duplicate.plist");
        write(duplicateRoot, plist(null, false));
        write(new File(assets, "duplicate.png"), "png");
        write(duplicateGameScene, plist(null, false));
        write(new File(assets, "gamescene/duplicate.png"), "png");
        List<GameSceneAtlasResolver.Atlas> duplicate =
                GameSceneAtlasResolver.resolve(assets, Arrays.asList("duplicate.plist"));
        assert duplicate != null && duplicate.size() == 1;
        assert duplicate.get(0).plistRelativePath.equals("duplicate.plist");
        assert duplicate.get(0).textureRelativePath.equals("duplicate.png");

        // Child-directory duplicates preserve the recovered order as well:
        // `other` precedes `sound` in AddAllSearchPath().
        write(new File(assets, "other/ordered.plist"), plist(null, false));
        write(new File(assets, "other/ordered.png"), "png");
        write(new File(assets, "sound/ordered.plist"), plist(null, false));
        write(new File(assets, "sound/ordered.png"), "png");
        List<GameSceneAtlasResolver.Atlas> ordered =
                GameSceneAtlasResolver.resolve(assets, Arrays.asList("ordered.plist"));
        assert ordered != null && ordered.size() == 1;
        assert ordered.get(0).plistRelativePath.equals("other/ordered.plist");

        // CCFileUtils::fullPathFromRelativeFile joins textureFileName to the
        // plist directory and permits parent-relative components. Preserve that
        // behavior when canonicalization still keeps the texture inside assets.
        File parentRelativePlist = new File(assets, "gamescene/l02/res/relative.plist");
        File parentRelativePng = new File(assets, "gamescene/l02/shared.png");
        write(parentRelativePlist, plist("../shared.png", false));
        write(parentRelativePng, "png");
        List<GameSceneAtlasResolver.Atlas> parentRelative =
                GameSceneAtlasResolver.resolve(assets, Arrays.asList("l02/res/relative.plist"));
        assert parentRelative != null && parentRelative.size() == 1;
        assert parentRelative.get(0).textureRelativePath.equals("gamescene/l02/shared.png");

        // Relative metadata may not escape the imported asset root even though
        // the original path join itself accepts `..` components.
        File escapingPlist = new File(assets, "gamescene/escape/escaping.plist");
        write(escapingPlist, plist("../../../outside-root.png", false));
        write(new File(root, "outside-root.png"), "png");
        assert GameSceneAtlasResolver.resolve(
                assets, Arrays.asList("escape/escaping.plist")) == null;

        // Absolute and Windows-style metadata paths remain rejected: the
        // project-owned importer is Android/Linux-rooted and never needs to
        // reinterpret those strings as a different platform's path syntax.
        File absolutePlist = new File(assets, "gamescene/absolute.plist");
        write(absolutePlist, plist("/tmp/outside.png", false));
        assert GameSceneAtlasResolver.resolve(
                assets, Arrays.asList("absolute.plist")) == null;
        File backslashPlist = new File(assets, "gamescene/backslash.plist");
        write(backslashPlist, plist("..\\shared.png", false));
        assert GameSceneAtlasResolver.resolve(
                assets, Arrays.asList("backslash.plist")) == null;

        File playerPlist = new File(assets,
                "player/Player01_Res/p01_01_01_default.plist");
        write(playerPlist, plist("p01_01_01_default.png", false));
        write(new File(playerPlist.getParentFile(), "p01_01_01_default.png"), "png");
        List<GameSceneAtlasResolver.Atlas> player =
                GameSceneAtlasResolver.resolvePlayerDefaults(assets, 1);
        assert player != null && player.size() == 1;
        assert player.get(0).plistRelativePath.equals(
                "player/Player01_Res/p01_01_01_default.plist");
        assert GameSceneAtlasResolver.resolvePlayerDefaults(assets, 2) == null;

        // MeActionsSystem loads the current ActionFrameData +0x60 plist directly
        // before asking spriteFrameByName() for +0x68. Resolve that exact basename
        // through the same recovered Cocos search order instead of scanning every
        // player default atlas and treating unrelated membership as ambiguity.
        File actionPlist = new File(assets,
                "player/Player01_Res/p01_02_default.plist");
        write(actionPlist, plist("p01_02_default.png", false));
        write(new File(actionPlist.getParentFile(), "p01_02_default.png"), "png");
        List<GameSceneAtlasResolver.Atlas> actionAtlas =
                GameSceneAtlasResolver.resolve(assets, Arrays.asList("p01_02_default.plist"));
        assert actionAtlas != null && actionAtlas.size() == 1;
        assert actionAtlas.get(0).plistRelativePath.equals(
                "player/Player01_Res/p01_02_default.plist");
        assert actionAtlas.get(0).textureRelativePath.equals(
                "player/Player01_Res/p01_02_default.png");

        deleteTree(root);
    }

    private static void deleteTree(File file) {
        File[] children = file.listFiles();
        if (children != null) for (File child : children) deleteTree(child);
        if (!file.delete()) throw new IllegalStateException("delete failed: " + file);
    }
}
