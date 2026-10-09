package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import java.io.File;
import java.util.Locale;

final class SingleSelectHeroBaseComposer {
    private static final String BACKGROUND_FRAME = "xrbeijing.png";

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

    private SingleSelectHeroBaseComposer() {}

    static SingleLoginAtlasComposer.AtlasLayer composeBackground(File plistFile, File atlasFile)
            throws Exception {
        nativeClearHeroTables();
        nativeClearCareerRunes();

        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        Bitmap atlas = BitmapFactory.decodeFile(atlasFile.getAbsolutePath(), options);
        if (atlas == null) return null;

        try {
            SingleLoginAtlasComposer.AtlasLayer background =
                    extractLayer(plistFile, atlas, BACKGROUND_FRAME);
            if (background == null) return null;

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
                    if (layer == null || !nativeUploadHeroTable(
                            outputIndex,
                            layer.width,
                            layer.height,
                            layer.left,
                            layer.top,
                            layer.sourceWidth,
                            layer.sourceHeight,
                            layer.pixels)) {
                        nativeClearHeroTables();
                        break;
                    }
                    outputIndex++;
                }
                if (outputIndex != career * 2) break;
            }

            // initUI() creates five tagged CCMenuItemSprite rune controls.
            // Each item has a normal xrfuwenNN frame and a pressed
            // xrfuwenfaguangNN frame. Items 3..5 remain visually present but
            // disabled in native state, matching the shipped menu.
            int runeIndex = 0;
            for (int tag = 1; tag <= 5; tag++) {
                String[] frameNames = {
                        String.format(Locale.ROOT, "xrfuwen%02d.png", tag),
                        String.format(Locale.ROOT, "xrfuwenfaguang%02d.png", tag),
                };
                for (String frameName : frameNames) {
                    SingleLoginAtlasComposer.AtlasLayer layer =
                            extractLayer(plistFile, atlas, frameName);
                    if (layer == null || !nativeUploadCareerRune(
                            runeIndex,
                            layer.width,
                            layer.height,
                            layer.left,
                            layer.top,
                            layer.sourceWidth,
                            layer.sourceHeight,
                            layer.pixels)) {
                        nativeClearCareerRunes();
                        runeIndex = -1;
                        break;
                    }
                    runeIndex++;
                }
                if (runeIndex < 0) break;
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
