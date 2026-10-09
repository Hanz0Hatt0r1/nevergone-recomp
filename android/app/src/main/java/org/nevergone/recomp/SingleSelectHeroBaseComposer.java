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
    private static native void nativeClearCareerRuneTouchAssets();
    private static native boolean nativeConfigureCareerRuneHitbox(
            int tag,
            int width,
            int height,
            int left,
            int top,
            int sourceWidth,
            int sourceHeight);
    private static native boolean nativeUploadCareerRuneGlow(
            int tag,
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
        nativeClearCareerRuneTouchAssets();

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

            // initUI() creates five tagged CCMenuItemSprite controls. Their
            // normal sprite content size is the native hit box, while the
            // xrfuwenfaguangNN selected sprite is used for pressed feedback.
            for (int tag = 1; tag <= 5; tag++) {
                SingleLoginAtlasComposer.AtlasLayer normal = extractLayer(
                        plistFile,
                        atlas,
                        String.format(Locale.ROOT, "xrfuwen%02d.png", tag));
                SingleLoginAtlasComposer.AtlasLayer glow = extractLayer(
                        plistFile,
                        atlas,
                        String.format(Locale.ROOT, "xrfuwenfaguang%02d.png", tag));
                if (normal == null || glow == null ||
                        !nativeUploadCareerRune(
                                (tag - 1) * 2,
                                normal.width,
                                normal.height,
                                normal.left,
                                normal.top,
                                normal.sourceWidth,
                                normal.sourceHeight,
                                normal.pixels) ||
                        !nativeUploadCareerRune(
                                (tag - 1) * 2 + 1,
                                glow.width,
                                glow.height,
                                glow.left,
                                glow.top,
                                glow.sourceWidth,
                                glow.sourceHeight,
                                glow.pixels) ||
                        !nativeConfigureCareerRuneHitbox(
                                tag,
                                normal.width,
                                normal.height,
                                normal.left,
                                normal.top,
                                normal.sourceWidth,
                                normal.sourceHeight) ||
                        !nativeUploadCareerRuneGlow(
                                tag,
                                glow.width,
                                glow.height,
                                glow.left,
                                glow.top,
                                glow.sourceWidth,
                                glow.sourceHeight,
                                glow.pixels)) {
                    nativeClearCareerRunes();
                    nativeClearCareerRuneTouchAssets();
                    break;
                }
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
