package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import java.io.File;

final class SingleSelectHeroBaseComposer {
    private static final String BACKGROUND_FRAME = "xrbeijing.png";

    private SingleSelectHeroBaseComposer() {}

    static SingleLoginAtlasComposer.AtlasLayer composeBackground(File plistFile, File atlasFile)
            throws Exception {
        TexturePackerPlist.Frame frame = TexturePackerPlist.readFrame(plistFile, BACKGROUND_FRAME);
        if (frame == null) return null;

        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        Bitmap atlas = BitmapFactory.decodeFile(atlasFile.getAbsolutePath(), options);
        if (atlas == null) return null;

        try {
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
        } finally {
            atlas.recycle();
        }
    }
}
