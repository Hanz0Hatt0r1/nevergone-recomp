package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import java.io.File;
import java.util.Locale;

final class SingleSelectHeroBaseComposer {
    private static final String BACKGROUND_FRAME = "xrbeijing.png";

    private static volatile boolean atlasDecoded;
    private static volatile boolean backgroundExtracted;
    private static volatile int heroTablesRetained;
    private static volatile int heroCharactersRetained;
    private static volatile int careerRunesRetained;
    private static volatile int confirmFramesRetained;
    private static volatile String heroTableFailure = "not staged";
    private static volatile String heroCharacterFailure = "not staged";
    private static volatile String careerRuneFailure = "not staged";
    private static volatile String confirmFailure = "not staged";

    private static native void nativeClearHeroTables();
    private static native boolean nativeUploadHeroTable(
            int tableIndex,
            int width,
            int height,
            int left,
            int top,
            int sourceWidth,
            int sourceHeight,
            int[] argbPixels);
    private static native void nativeClearHeroCharacters();
    private static native boolean nativeUploadHeroCharacter(
            int frameIndex,
            int width,
            int height,
            int left,
            int top,
            int sourceWidth,
            int sourceHeight,
            int[] argbPixels);
    private static native void nativeClearCareerRunes();
    private static native boolean nativeUploadCareerRune(
            int runeIndex,
            int width,
            int height,
            int left,
            int top,
            int sourceWidth,
            int sourceHeight,
            int[] argbPixels);
    private static native void nativeClearConfirmControl();
    private static native boolean nativeUploadConfirmFrame(
            int frameIndex,
            int width,
            int height,
            int left,
            int top,
            int sourceWidth,
            int sourceHeight,
            int[] argbPixels);

    private SingleSelectHeroBaseComposer() {}

    private static void resetStatus() {
        atlasDecoded = false;
        backgroundExtracted = false;
        heroTablesRetained = 0;
        heroCharactersRetained = 0;
        careerRunesRetained = 0;
        confirmFramesRetained = 0;
        heroTableFailure = "none";
        heroCharacterFailure = "none";
        careerRuneFailure = "none";
        confirmFailure = "none";
    }

    static String statusReport() {
        return "SingleSelectHero staging\n" +
                "atlas decoded: " + (atlasDecoded ? "yes" : "no") + "\n" +
                "background extracted: " + (backgroundExtracted ? "yes" : "no") + "\n" +
                "hero tables retained: " + heroTablesRetained + "/4\n" +
                "hero character frames retained: " + heroCharactersRetained + "/6\n" +
                "career runes retained: " + careerRunesRetained + "/10\n" +
                "confirm frames retained: " + confirmFramesRetained + "/4\n" +
                "hero table failure: " + heroTableFailure + "\n" +
                "hero character failure: " + heroCharacterFailure + "\n" +
                "career rune failure: " + careerRuneFailure + "\n" +
                "confirm failure: " + confirmFailure;
    }

    static void clearStagedFrames() {
        nativeClearHeroTables();
        nativeClearHeroCharacters();
        nativeClearCareerRunes();
        nativeClearConfirmControl();
        resetStatus();
    }

    static SingleLoginAtlasComposer.AtlasLayer composeBackground(File plistFile, File atlasFile)
            throws Exception {
        nativeClearHeroTables();
        nativeClearHeroCharacters();
        nativeClearCareerRunes();
        nativeClearConfirmControl();
        resetStatus();

        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        Bitmap atlas = BitmapFactory.decodeFile(atlasFile.getAbsolutePath(), options);
        if (atlas == null) {
            heroTableFailure = "atlas decode failed";
            heroCharacterFailure = "atlas decode failed";
            careerRuneFailure = "atlas decode failed";
            confirmFailure = "atlas decode failed";
            return null;
        }
        atlasDecoded = true;

        try {
            SingleLoginAtlasComposer.AtlasLayer background =
                    extractLayer(plistFile, atlas, BACKGROUND_FRAME);
            if (background == null) {
                heroTableFailure = "background frame missing or invalid";
                heroCharacterFailure = "background frame missing or invalid";
                careerRuneFailure = "background frame missing or invalid";
                confirmFailure = "background frame missing or invalid";
                return null;
            }
            backgroundExtracted = true;

            // HeroTable is part of the same shipped atlas and is loaded on this
            // GL thread so the native compositor can select frames at runtime
            // without retaining proprietary bytes in the project.
            final String prefix = SingleSelectHeroPictureLanguage.prefix(Locale.getDefault());
            int outputIndex = 0;
            for (int career = 1; career <= 2; career++) {
                for (char variant : new char[] {'a', 'b'}) {
                    String frameName = String.format(
                            Locale.ROOT,
                            "%s_HeroTable_%02d_%c.png",
                            prefix,
                            career,
                            variant);
                    SingleLoginAtlasComposer.AtlasLayer layer =
                            extractLayer(plistFile, atlas, frameName);
                    if (layer == null) {
                        heroTableFailure = "missing/invalid " + frameName +
                                " after " + outputIndex + " accepted";
                        nativeClearHeroTables();
                        heroTablesRetained = 0;
                        break;
                    }
                    if (!nativeUploadHeroTable(
                            outputIndex,
                            layer.width,
                            layer.height,
                            layer.left,
                            layer.top,
                            layer.sourceWidth,
                            layer.sourceHeight,
                            layer.pixels)) {
                        heroTableFailure = "native upload rejected " + frameName +
                                " after " + outputIndex + " accepted";
                        nativeClearHeroTables();
                        heroTablesRetained = 0;
                        break;
                    }
                    outputIndex++;
                    heroTablesRetained = outputIndex;
                }
                if (outputIndex != career * 2) break;
            }

            // Preview bodies belong to this APK atlas, not to the OBB player
            // action atlases used later by GameScene/MEPlayer.
            final String[] heroCharacterFrames = {
                    "P01DL_Ani_0001.png",
                    "P01DL_Ani_0001_grey.png",
                    "P02DL_Ani_0001.png",
                    "P02DL_Ani_0001_grey.png",
                    "P01DL_Ani_0001b.png",
                    "P02DL_Ani_0001b.png",
            };
            for (int frameIndex = 0; frameIndex < heroCharacterFrames.length; frameIndex++) {
                final String frameName = heroCharacterFrames[frameIndex];
                SingleLoginAtlasComposer.AtlasLayer layer =
                        extractLayer(plistFile, atlas, frameName);
                if (layer == null) {
                    heroCharacterFailure = "missing/invalid " + frameName +
                            " after " + frameIndex + " accepted";
                    nativeClearHeroCharacters();
                    heroCharactersRetained = 0;
                    break;
                }
                if (!nativeUploadHeroCharacter(
                        frameIndex,
                        layer.width,
                        layer.height,
                        layer.left,
                        layer.top,
                        layer.sourceWidth,
                        layer.sourceHeight,
                        layer.pixels)) {
                    heroCharacterFailure = "native upload rejected " + frameName +
                            " after " + frameIndex + " accepted";
                    nativeClearHeroCharacters();
                    heroCharactersRetained = 0;
                    break;
                }
                heroCharactersRetained = frameIndex + 1;
            }

            // initUI() creates five tagged CCMenuItemSprite rune controls.
            // Each item has a normal xrfuwenNN frame and a pressed
            // xrfuwenfaguangNN frame; all five recovered career tags are valid.
            int runeIndex = 0;
            boolean runeFailed = false;
            for (int tag = 1; tag <= 5; tag++) {
                String[] frameNames = {
                        String.format(Locale.ROOT, "xrfuwen%02d.png", tag),
                        String.format(Locale.ROOT, "xrfuwenfaguang%02d.png", tag),
                };
                for (String frameName : frameNames) {
                    SingleLoginAtlasComposer.AtlasLayer layer =
                            extractLayer(plistFile, atlas, frameName);
                    if (layer == null) {
                        careerRuneFailure = "missing/invalid " + frameName +
                                " after " + runeIndex + " accepted";
                        runeFailed = true;
                    } else if (!nativeUploadCareerRune(
                            runeIndex,
                            layer.width,
                            layer.height,
                            layer.left,
                            layer.top,
                            layer.sourceWidth,
                            layer.sourceHeight,
                            layer.pixels)) {
                        careerRuneFailure = "native upload rejected " + frameName +
                                " after " + runeIndex + " accepted";
                        runeFailed = true;
                    }
                    if (runeFailed) {
                        nativeClearCareerRunes();
                        careerRunesRetained = 0;
                        break;
                    }
                    runeIndex++;
                    careerRunesRetained = runeIndex;
                }
                if (runeFailed) break;
            }

            // Recovered Confirm hierarchy at the shared (836,70) center:
            // btn_a parent -> btn_b child -> btn_d/btn_e menu item.
            String[] confirmFrames = {
                    "btn_a.png",
                    "btn_b.png",
                    "btn_d.png",
                    "btn_e.png",
            };
            for (int frameIndex = 0; frameIndex < confirmFrames.length; frameIndex++) {
                String frameName = confirmFrames[frameIndex];
                SingleLoginAtlasComposer.AtlasLayer layer =
                        extractLayer(plistFile, atlas, frameName);
                if (layer == null) {
                    confirmFailure = "missing/invalid " + frameName +
                            " after " + frameIndex + " accepted";
                    nativeClearConfirmControl();
                    confirmFramesRetained = 0;
                    break;
                }
                if (!nativeUploadConfirmFrame(
                        frameIndex,
                        layer.width,
                        layer.height,
                        layer.left,
                        layer.top,
                        layer.sourceWidth,
                        layer.sourceHeight,
                        layer.pixels)) {
                    confirmFailure = "native upload rejected " + frameName +
                            " after " + frameIndex + " accepted";
                    nativeClearConfirmControl();
                    confirmFramesRetained = 0;
                    break;
                }
                confirmFramesRetained = frameIndex + 1;
            }
            return background;
        } finally {
            atlas.recycle();
        }
    }

    private static SingleLoginAtlasComposer.AtlasLayer extractLayer(
            File plistFile,
            Bitmap atlas,
            String frameName) throws Exception {
        TexturePackerPlist.Frame frame = TexturePackerPlist.readFrame(plistFile, frameName);
        if (frame == null) return null;
        TexturePackerAtlasExtractor.ExtractedFrame extracted =
                TexturePackerAtlasExtractor.extract(frame, atlas);
        if (extracted == null) return null;
        return new SingleLoginAtlasComposer.AtlasLayer(
                extracted.width,
                extracted.height,
                extracted.left,
                extracted.top,
                extracted.sourceWidth,
                extracted.sourceHeight,
                extracted.pixels);
    }
}
