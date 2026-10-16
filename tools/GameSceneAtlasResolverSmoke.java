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

        // CCSpriteFrameCache resolves the plist and its texture in two distinct
        // CCFileUtils searches. The plist can therefore come from a later root
        // while the logical texture name resolves from an earlier root.
        write(new File(assets, "other/cross-root.plist"),
                plist("cross-root.png", false));
        write(new File(assets, "other/cross-root.png"), "later-texture");
        write(new File(assets, "player/cross-root.png"), "earlier-texture");
        List<GameSceneAtlasResolver.Atlas> crossRoot =
                GameSceneAtlasResolver.resolve(assets, Arrays.asList("cross-root.plist"));
        assert crossRoot != null && crossRoot.size() == 1;
        assert crossRoot.get(0).plistRelativePath.equals("other/cross-root.plist");
        assert crossRoot.get(0).textureRelativePath.equals("player/cross-root.png");

        // The extension-replacement fallback follows the same independent
        // search. It must not be pinned to the physical plist directory either.
        write(new File(assets, "other/cross-fallback.plist"), plist(null, false));
        write(new File(assets, "other/cross-fallback.png"), "later-texture");
        write(new File(assets, "player/cross-fallback.png"), "earlier-texture");
        List<GameSceneAtlasResolver.Atlas> crossFallback =
                GameSceneAtlasResolver.resolve(assets, Arrays.asList("cross-fallback.plist"));
        assert crossFallback != null && crossFallback.size() == 1;
        assert crossFallback.get(0).plistRelativePath.equals("other/cross-fallback.plist");
        assert crossFallback.get(0).textureRelativePath.equals("player/cross-fallback.png");

        // CCFileUtils::fullPathFromRelativeFile joins textureFileName to the
        // logical plist directory and permits parent-relative components. Keep
        // that behavior when canonicalization stays inside imported assets.
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

        // CCSprite::create(file) uses the same CCFileUtils search path ordering.
        // A direct sprite basename that exists only under controls_ui must resolve
        // even though the stager is rooted at the top-level imported asset tree.
        File directSearchSprite = new File(assets, "controls_ui/direct-search.png");
        write(directSearchSprite, "png");
        File directSearchResolved =
                GameSceneAtlasResolver.resolveResource(assets, "direct-search.png");
        assert directSearchResolved != null;
        assert directSearchResolved.getCanonicalPath().equals(
                directSearchSprite.getCanonicalPath());

        // Root-first precedence applies to direct sprites too.
        File directRootSprite = new File(assets, "direct-duplicate.png");
        File directOtherSprite = new File(assets, "other/direct-duplicate.png");
        write(directRootSprite, "root");
        write(directOtherSprite, "other");
        File directDuplicateResolved =
                GameSceneAtlasResolver.resolveResource(assets, "direct-duplicate.png");
        assert directDuplicateResolved != null;
        assert directDuplicateResolved.getCanonicalPath().equals(
                directRootSprite.getCanonicalPath());

        // Tutorial resources live below the recovered gamescene_ui search root.
        // Their logical names must resolve without hard-coding that physical root.
        File lessonImage = new File(assets,
                "gamescene_ui/lessonsUI/keyboard_lesson.png");
        File dialogueImage = new File(assets,
                "gamescene_ui/Dialogue_UI/dialogue_bg.png");
        write(lessonImage, "png");
        write(dialogueImage, "png");
        assert GameSceneAtlasResolver.resolveResource(
                assets, "lessonsUI/keyboard_lesson.png").getCanonicalPath().equals(
                lessonImage.getCanonicalPath());
        assert GameSceneAtlasResolver.resolveResource(
                assets, "Dialogue_UI/dialogue_bg.png").getCanonicalPath().equals(
                dialogueImage.getCanonicalPath());

        // Project-owned lookup stays fail-closed for missing or unsafe names.
        assert GameSceneAtlasResolver.resolveResource(assets, "missing-direct.png") == null;
        assert GameSceneAtlasResolver.resolveResource(assets, "../outside-root.png") == null;
        assert GameSceneAtlasResolver.resolveResource(assets, "/tmp/outside-root.png") == null;

        deleteTree(root);
    }

    private static void deleteTree(File file) {
        File[] children = file.listFiles();
        if (children != null) for (File child : children) deleteTree(child);
        if (!file.delete()) throw new IllegalStateException("delete failed: " + file);
    }
}
