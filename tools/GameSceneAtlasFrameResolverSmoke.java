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
        String colorRectXml = colorRect != null
                ? "<key>spriteColorRect</key><string>" + colorRect + "</string>"
                : "";
        return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
                "<plist version=\"1.0\"><dict>" +
                "<key>frames</key><dict>" +
                "<key>" + frameName + "</key><dict>" +
                "<key>textureRect</key><string>" + textureRect + "</string>" +
                colorRectXml +
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
                            "{{15,15},{30,40}}",
                            "{-2,5}",
                            "{30,40}",
                            "{64,80}",
                            true));

            GameSceneAtlasFrameResolver.Prepared prepared =
                    GameSceneAtlasFrameResolver.prepare(
                            root, Collections.singletonList(atlas));
            assert prepared != null;

            GameSceneAtlasFrameResolver.Frame exact = prepared.resolve("hero_idle.png");
            assert exact != null;
            assert exact.atlasIndex == 0;
            assert exact.requestedName.equals("hero_idle.png");
            assert exact.canonicalName.equals("hero_idle.png");
            assert exact.textureX == 10 && exact.textureY == 20;
            assert exact.textureWidth == 30 && exact.textureHeight == 40;
            // color geometry is derived from source size + sprite size + offset,
            // matching the fields cocos2d-x 2.1.2 actually consumes.
            assert exact.colorX == 15 && exact.colorY == 15;
            assert exact.colorWidth == 30 && exact.colorHeight == 40;
            assert exact.offsetX == -2 && exact.offsetY == 5;
            assert exact.spriteWidth == 30 && exact.spriteHeight == 40;
            assert exact.sourceWidth == 64 && exact.sourceHeight == 80;
            assert exact.rotated;

            GameSceneAtlasFrameResolver.Frame alias = prepared.resolve("hero_alias.png");
            assert alias != null;
            assert alias.canonicalName.equals("hero_idle.png");
            assert alias.requestedName.equals("hero_alias.png");
            assert prepared.resolve("missing.png") == null;

            // The compatibility one-shot API uses the same prepared path.
            GameSceneAtlasFrameResolver.Frame oneShot = GameSceneAtlasFrameResolver.resolve(
                    root, Collections.singletonList(atlas), "hero_idle.png");
            assert oneShot != null;
            assert oneShot.canonicalName.equals("hero_idle.png");

            // cocos2d-x 2.1.2 keeps the first canonical frame loaded under a
            // duplicate key and skips the later frame before processing its
            // aliases. Match that cache precedence exactly.
            GameSceneAtlasResolver.Atlas duplicate = writeAtlas(
                    root,
                    "gamescene/gs_actions/res/b",
                    plist(
                            "hero_idle.png", "<string>duplicate_alias.png</string>",
                            "{{0,0},{1,1}}", "{{0,0},{1,1}}",
                            "{0,0}", "{1,1}", "{1,1}", false));
            GameSceneAtlasFrameResolver.Prepared duplicatePrepared =
                    GameSceneAtlasFrameResolver.prepare(root, Arrays.asList(atlas, duplicate));
            assert duplicatePrepared != null;
            GameSceneAtlasFrameResolver.Frame duplicateWinner =
                    duplicatePrepared.resolve("hero_idle.png");
            assert duplicateWinner != null;
            assert duplicateWinner.atlasIndex == 0;
            assert duplicateWinner.textureX == 10;
            assert duplicatePrepared.resolve("duplicate_alias.png") == null;

            // Aliases are different in the shipped cache: accepted later frames
            // overwrite an earlier alias mapping even though canonical frame
            // duplicates themselves never overwrite.
            GameSceneAtlasResolver.Atlas aliasOverride = writeAtlas(
                    root,
                    "gamescene/gs_actions/res/d",
                    plist(
                            "hero_second.png", "<string>hero_alias.png</string>",
                            "{{1,2},{3,4}}", "{{0,0},{3,4}}",
                            "{0,0}", "{3,4}", "{3,4}", false));
            GameSceneAtlasFrameResolver.Prepared aliasPrepared =
                    GameSceneAtlasFrameResolver.prepare(root, Arrays.asList(atlas, aliasOverride));
            assert aliasPrepared != null;
            GameSceneAtlasFrameResolver.Frame aliasWinner =
                    aliasPrepared.resolve("hero_alias.png");
            assert aliasWinner != null;
            assert aliasWinner.atlasIndex == 1;
            assert aliasWinner.canonicalName.equals("hero_second.png");

            // A real canonical name always wins lookup before the alias map.
            GameSceneAtlasResolver.Atlas canonicalOverride = writeAtlas(
                    root,
                    "gamescene/gs_actions/res/e",
                    plist(
                            "hero_alias.png", "",
                            "{{5,6},{7,8}}", "{{0,0},{7,8}}",
                            "{0,0}", "{7,8}", "{7,8}", false));
            GameSceneAtlasFrameResolver.Prepared canonicalPrepared =
                    GameSceneAtlasFrameResolver.prepare(
                            root, Arrays.asList(atlas, aliasOverride, canonicalOverride));
            assert canonicalPrepared != null;
            GameSceneAtlasFrameResolver.Frame canonicalWinner =
                    canonicalPrepared.resolve("hero_alias.png");
            assert canonicalWinner != null;
            assert canonicalWinner.atlasIndex == 2;
            assert canonicalWinner.canonicalName.equals("hero_alias.png");

            // spriteColorRect is not consumed by the original format-3 loader.
            // A valid player/action plist may omit it entirely; derive the trim
            // from the four authoritative geometry fields instead.
            GameSceneAtlasResolver.Atlas noColorRect = writeAtlas(
                    root,
                    "player/Player01_Res/no-color",
                    plist(
                            "player_frame.png", "",
                            "{{20,30},{12,20}}", null,
                            "{2,-3}", "{12,20}", "{20,30}", false));
            GameSceneAtlasFrameResolver.Prepared noColorPrepared =
                    GameSceneAtlasFrameResolver.prepare(
                            root, Collections.singletonList(noColorRect));
            assert noColorPrepared != null;
            GameSceneAtlasFrameResolver.Frame noColor =
                    noColorPrepared.resolve("player_frame.png");
            assert noColor != null;
            assert noColor.colorX == 6;
            assert noColor.colorY == 8;
            assert noColor.colorWidth == 12;
            assert noColor.colorHeight == 20;

            // Even if an exporter writes a redundant spriteColorRect that does
            // not match, original cocos2d-x ignores it. The clean resolver must
            // likewise use the authoritative size/offset geometry.
            GameSceneAtlasResolver.Atlas ignoredColorRect = writeAtlas(
                    root,
                    "player/Player01_Res/ignored-color",
                    plist(
                            "ignored_color.png", "",
                            "{{0,0},{4,6}}", "{{99,99},{1,1}}",
                            "{1,2}", "{4,6}", "{10,12}", false));
            GameSceneAtlasFrameResolver.Prepared ignoredColorPrepared =
                    GameSceneAtlasFrameResolver.prepare(
                            root, Collections.singletonList(ignoredColorRect));
            assert ignoredColorPrepared != null;
            GameSceneAtlasFrameResolver.Frame ignoredColor =
                    ignoredColorPrepared.resolve("ignored_color.png");
            assert ignoredColor != null;
            assert ignoredColor.colorX == 4;
            assert ignoredColor.colorY == 1;

            // Reject authoritative trim geometry that escapes the source.
            GameSceneAtlasResolver.Atlas malformed = writeAtlas(
                    root,
                    "gamescene/gs_actions/res/c",
                    plist(
                            "bad.png", "", "{{0,0},{8,8}}", null,
                            "{7,0}", "{8,8}", "{10,10}", false));
            GameSceneAtlasFrameResolver.Prepared malformedPrepared =
                    GameSceneAtlasFrameResolver.prepare(
                            root, Collections.singletonList(malformed));
            assert malformedPrepared != null;
            assert malformedPrepared.resolve("bad.png") == null;

            // Resolved atlas paths remain confined to the imported asset root.
            GameSceneAtlasResolver.Atlas traversal =
                    new GameSceneAtlasResolver.Atlas("../outside.plist", "../outside.png");
            assert GameSceneAtlasFrameResolver.prepare(
                    root, Collections.singletonList(traversal)) == null;
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
