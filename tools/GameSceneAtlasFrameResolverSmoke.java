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
                "<key>metadata</key><dict>" +
                "<key>format</key><integer>3</integer>" +
                "<key>textureFileName</key><string>sheet.png</string>" +
                "</dict></dict></plist>";
    }

    private static String legacyPlist(int format, String frameName, String frameBody) {
        return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
                "<plist version=\"1.0\"><dict>" +
                "<key>frames</key><dict><key>" + frameName + "</key><dict>" +
                frameBody +
                "</dict></dict>" +
                "<key>metadata</key><dict>" +
                "<key>format</key><integer>" + format + "</integer>" +
                "<key>textureFileName</key><string>sheet.png</string>" +
                "</dict></dict></plist>";
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
            // True cache misses remain null so ordinary GameScene staging can
            // reproduce the original empty.png + setVisible(false) fallback.
            assert prepared.resolve("missing.png") == null;

            GameSceneAtlasFrameResolver.Frame oneShot = GameSceneAtlasFrameResolver.resolve(
                    root, Collections.singletonList(atlas), "hero_idle.png");
            assert oneShot != null;
            assert oneShot.canonicalName.equals("hero_idle.png");

            // cocos2d-x 2.1.2 keeps the first canonical frame loaded under a
            // duplicate key and skips the later frame before processing aliases.
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

            // Format 3 does not consume spriteColorRect.
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

            // Format 0: exact legacy x/y/width/height + offsets/original size.
            // Numeric XML values may be written as integral-valued decimals;
            // the integer pixel path accepts them without rounding.
            GameSceneAtlasResolver.Atlas format0 = writeAtlas(
                    root,
                    "player/Player01_Res/format0",
                    legacyPlist(
                            0,
                            "legacy0.png",
                            "<key>x</key><real>10.0</real>" +
                            "<key>y</key><real>20.0</real>" +
                            "<key>width</key><real>30.0</real>" +
                            "<key>height</key><real>40.0</real>" +
                            "<key>offsetX</key><real>-2.0</real>" +
                            "<key>offsetY</key><real>5.0</real>" +
                            "<key>originalWidth</key><integer>-64</integer>" +
                            "<key>originalHeight</key><integer>-80</integer>"));
            GameSceneAtlasFrameResolver.Prepared format0Prepared =
                    GameSceneAtlasFrameResolver.prepare(
                            root, Collections.singletonList(format0));
            assert format0Prepared != null;
            GameSceneAtlasFrameResolver.Frame legacy0 = format0Prepared.resolve("legacy0.png");
            assert legacy0 != null;
            assert legacy0.textureX == 10 && legacy0.textureY == 20;
            assert legacy0.spriteWidth == 30 && legacy0.spriteHeight == 40;
            assert legacy0.sourceWidth == 64 && legacy0.sourceHeight == 80;
            assert legacy0.offsetX == -2 && legacy0.offsetY == 5;
            assert !legacy0.rotated;

            // Format 1: frame/offset/sourceSize, never rotated.
            GameSceneAtlasResolver.Atlas format1 = writeAtlas(
                    root,
                    "player/Player01_Res/format1",
                    legacyPlist(
                            1,
                            "legacy1.png",
                            "<key>frame</key><string>{{10.0,20.0},{30.0,40.0}}</string>" +
                            "<key>offset</key><string>{-2.0,5.0}</string>" +
                            "<key>sourceSize</key><string>{64.0,80.0}</string>"));
            GameSceneAtlasFrameResolver.Prepared format1Prepared =
                    GameSceneAtlasFrameResolver.prepare(
                            root, Collections.singletonList(format1));
            assert format1Prepared != null;
            GameSceneAtlasFrameResolver.Frame legacy1 = format1Prepared.resolve("legacy1.png");
            assert legacy1 != null;
            assert legacy1.textureX == 10 && legacy1.textureY == 20;
            assert legacy1.spriteWidth == 30 && legacy1.spriteHeight == 40;
            assert !legacy1.rotated;

            // Format 2 adds the rotated flag to the format-1 schema.
            GameSceneAtlasResolver.Atlas format2 = writeAtlas(
                    root,
                    "player/Player01_Res/format2",
                    legacyPlist(
                            2,
                            "legacy2.png",
                            "<key>frame</key><string>{{10,20},{30,40}}</string>" +
                            "<key>rotated</key><true/>" +
                            "<key>offset</key><string>{-2,5}</string>" +
                            "<key>sourceSize</key><string>{64,80}</string>"));
            GameSceneAtlasFrameResolver.Prepared format2Prepared =
                    GameSceneAtlasFrameResolver.prepare(
                            root, Collections.singletonList(format2));
            assert format2Prepared != null;
            GameSceneAtlasFrameResolver.Frame legacy2 = format2Prepared.resolve("legacy2.png");
            assert legacy2 != null;
            assert legacy2.rotated;
            assert legacy2.spriteWidth == 30 && legacy2.spriteHeight == 40;

            // Do not silently round genuinely fractional texel geometry. The
            // name is present, so the result must be non-null and invalid for
            // rendering; this prevents the scene hidden-fallback path from
            // confusing malformed metadata with a cache miss.
            GameSceneAtlasResolver.Atlas fractional = writeAtlas(
                    root,
                    "player/Player01_Res/fractional",
                    legacyPlist(
                            2,
                            "fractional.png",
                            "<key>frame</key><string>{{10.5,20},{30,40}}</string>" +
                            "<key>rotated</key><false/>" +
                            "<key>offset</key><string>{0,0}</string>" +
                            "<key>sourceSize</key><string>{30,40}</string>"));
            GameSceneAtlasFrameResolver.Prepared fractionalPrepared =
                    GameSceneAtlasFrameResolver.prepare(
                            root, Collections.singletonList(fractional));
            assert fractionalPrepared != null;
            GameSceneAtlasFrameResolver.Frame fractionalInvalid =
                    fractionalPrepared.resolve("fractional.png");
            assert fractionalInvalid != null;
            assert fractionalInvalid.spriteWidth == 0;
            assert fractionalInvalid.spriteHeight == 0;
            assert fractionalInvalid.sourceWidth == 0;
            assert fractionalInvalid.sourceHeight == 0;

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
            GameSceneAtlasFrameResolver.Frame malformedInvalid =
                    malformedPrepared.resolve("bad.png");
            assert malformedInvalid != null;
            assert malformedInvalid.canonicalName.equals("bad.png");
            assert malformedInvalid.spriteWidth == 0;
            assert malformedInvalid.sourceWidth == 0;
            assert malformedPrepared.resolve("actually_missing.png") == null;

            GameSceneAtlasResolver.Atlas unsupported = writeAtlas(
                    root,
                    "player/Player01_Res/format4",
                    legacyPlist(
                            4,
                            "unsupported.png",
                            "<key>frame</key><string>{{0,0},{1,1}}</string>"));
            assert GameSceneAtlasFrameResolver.prepare(
                    root, Collections.singletonList(unsupported)) == null;

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
