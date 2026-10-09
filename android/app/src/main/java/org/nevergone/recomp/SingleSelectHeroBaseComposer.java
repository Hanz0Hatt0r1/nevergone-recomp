package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import java.io.File;
import java.util.Locale;

final class SingleSelectHeroBaseComposer {
    private static final String BACKGROUND_FRAME = "xrbeijing.png";

    static final class SceneAssets {
        final SingleLoginAtlasComposer.AtlasLayer background;
        // Index: career 1 a/b, career 2 a/b.
        final SingleLoginAtlasComposer.AtlasLayer[] heroTables;

        SceneAssets(
                SingleLoginAtlasComposer.AtlasLayer background,
                SingleLoginAtlasComposer.AtlasLayer[] heroTables) {
            this.background = background;
            this.heroTables = heroTables;
        }
    }

    private SingleSelectHeroBaseComposer() {}

    static SceneAssets composeScene(File plistFile, File atlasFile, Locale locale)
            throws Exception {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        Bitmap atlas = BitmapFactory.decodeFile(atlasFile.getAbsolutePath(), options);
        if (atlas == null) return null;

        try {
            SingleLoginAtlasComposer.AtlasLayer background =
                    extractLayer(plistFile, atlas, BACKGROUND_FRAME);
            if (background == null) return null;

            final String prefix = picturePrefix(locale);
            SingleLoginAtlasComposer.AtlasLayer[] heroTables = new SingleLoginAtlasComposer.AtlasLayer[4];
            int outputIndex = 0;
            for (int career = 1; career <= 2; career++) {
                for (char variant : new char[] {'a', 'b'}) {
                    String frameName = String.format(
                            Locale.ROOT,
                            "%s_HeroTable_%02d_%c.png",
                            prefix,
                            career,
                            variant);
                    heroTables[outputIndex++] = extractLayer(plistFile, atlas, frameName);
                }
            }
            for (SingleLoginAtlasComposer.AtlasLayer layer : heroTables) {
                if (layer == null) return null;
            }
            return new SceneAssets(background, heroTables);
        } finally {
            atlas.recycle();
        }
    }

    static String picturePrefix(Locale locale) {
        if (locale == null) return "EN";
        String language = locale.getLanguage();
        // ManagementLayer::GetMultilingualPicturesName() maps the shipped
        // SystemLanguage enum 2 to CN, enum 5 to KR, and every other value to EN.
        if ("zh".equalsIgnoreCase(language)) return "CN";
        if ("ko".equalsIgnoreCase(language)) return "KR";
        return "EN";
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
