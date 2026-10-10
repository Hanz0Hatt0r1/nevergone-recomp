package org.nevergone.recomp;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.Arrays;
import java.util.Collections;

public final class GameSceneAtlasFrameResolverSmoke {
    private static String plist(
            String frameName,
            String aliases,
            String textureRect,
            String colorRect,
            String offset,
            String spriteSize,
            String sourceSize,
            boolean rotated) {
        return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
                "<plist version=\"1.0\"><dict>" +
                "<key>frames</key><dict>" +
                "<key>" + frameName + "</key><dict>" +
                "<key>textureRect</key><string>" + textureRect + "</string>" +
                "<key>spriteColorRect</key><string>" + colorRect + "</string>" +
                "<key>spriteOffset</key><string>" + offset + "</string>" +
                "<key>spriteSize</key><string>" + spriteSize + "</string>" +
                "<key>spriteSourceSize</key><string>" + sourceSize + "</string>" +
                "<key>textureRotated</key>" + (rotated ? "<true/>" : "<false/>") +
                "<key>aliases</key><array>" + aliases + "</array>" +
                "</dict></dict>" +
                "<key>metadata</key><dict><key>textureFileName</key><string>sheet.png</string></dict>" +
                "</dict></plist>";
    }

    private static GameSceneAtlasResolver.Atlas writeAtlas(
            File root, String directory, String xml) throws Exception {
        File dir = new File(root, directory);
        assert dir.mkdirs() || dir.isDirectory();
        File plist = new File(dir, "sheet.plist");
        File texture = new File(dir, "sheet.png");
        Files.write(plist.toPath(), xml.getBytes(StandardCharsets.UTF_8));
        Files.write(texture.toPath(), new byte[]{1});
        String plistRelative = root.toURI().relativize(plist.toURI()).getPath();
        String textureRelative = root.toURI().relativize(texture.toURI()).getPath();
        return new GameSceneAtlasResolver.Atlas(plistRelative, textureRelative);
    }

    public static void main(String[] args) throws Exception {
        File root = Files.createTempDirectory("nevergone-frame-resolver").toFile();
        try {
            GameSceneAtlasResolver.Atlas atlas = writeAtlas(
                    root,
                    "gamescene/gs_actions/res/a",
                    plist(
                            "hero_idle.png",
                            "<string>hero_alias.png</string>",
                            "{{10,20},{30,40}}",
                            "{{3,4},{30,40}}",
                            "{-2,5}",
                            "{30,40}",
                            "{64,80}",
                            true));

            GameSceneAtlasFrameResolver.Frame exact = GameSceneAtlasFrameResolver.resolve(
                    root, Collections.singletonList(atlas), "hero_idle.png");
            assert exact != null;
            assert exact.atlasIndex == 0;
            assert exact.requestedName.equals("hero_idle.png");
            assert exact.canonicalName.equals("hero_idle.png");
            assert exact.textureX == 10 && exact.textureY == 20;
            assert exact.textureWidth == 30 && exact.textureHeight == 40;
            assert exact.colorX == 3 && exact.colorY == 4;
            assert exact.colorWidth == 30 && exact.colorHeight == 40;
            assert exact.offsetX == -2 && exact.offsetY == 5;
            assert exact.spriteWidth == 30 && exact.spriteHeight == 40;
            assert exact.sourceWidth == 64 && exact.sourceHeight == 80;
            assert exact.rotated;

            GameSceneAtlasFrameResolver.Frame alias = GameSceneAtlasFrameResolver.resolve(
                    root, Collections.singletonList(atlas), "hero_alias.png");
            assert alias != null;
            assert alias.canonicalName.equals("hero_idle.png");
            assert alias.requestedName.equals("hero_alias.png");

            assert GameSceneAtlasFrameResolver.resolve(
                    root, Collections.singletonList(atlas), "missing.png") == null;

            // Duplicate membership across preloaded atlases is ambiguous at the
            // clean-room boundary and must fail closed instead of inventing an
            // overwrite/priority rule.
            GameSceneAtlasResolver.Atlas duplicate = writeAtlas(
                    root,
                    "gamescene/gs_actions/res/b",
                    plist(
                            "hero_idle.png", "", "{{0,0},{1,1}}", "{{0,0},{1,1}}",
                            "{0,0}", "{1,1}", "{1,1}", false));
            assert GameSceneAtlasFrameResolver.resolve(
                    root, Arrays.asList(atlas, duplicate), "hero_idle.png") == null;

            // Reject malformed trim geometry that escapes the declared source.
            GameSceneAtlasResolver.Atlas malformed = writeAtlas(
                    root,
                    "gamescene/gs_actions/res/c",
                    plist(
                            "bad.png", "", "{{0,0},{8,8}}", "{{7,7},{8,8}}",
                            "{0,0}", "{8,8}", "{10,10}", false));
            assert GameSceneAtlasFrameResolver.resolve(
                    root, Collections.singletonList(malformed), "bad.png") == null;

            // Resolved atlas paths remain confined to the imported asset root.
            GameSceneAtlasResolver.Atlas traversal =
                    new GameSceneAtlasResolver.Atlas("../outside.plist", "../outside.png");
            assert GameSceneAtlasFrameResolver.resolve(
                    root, Collections.singletonList(traversal), "hero_idle.png") == null;
        } finally {
            deleteTree(root);
        }
    }

    private static void deleteTree(File file) throws Exception {
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children != null) {
                for (File child : children) deleteTree(child);
            }
        }
        Files.deleteIfExists(file.toPath());
    }
}
