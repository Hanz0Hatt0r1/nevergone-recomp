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
        if (frame == null || frame.rotated || frame.textureWidth <= 0 || frame.textureHeight <= 0 ||
                frame.sourceWidth <= 0 || frame.sourceHeight <= 0 ||
                ((long) frame.sourceWidth * (long) frame.sourceHeight) > 16_777_216L) {
            return null;
        }

        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        Bitmap atlas = BitmapFactory.decodeFile(atlasFile.getAbsolutePath(), options);
        if (atlas == null) return null;

        try {
            if (frame.textureX < 0 || frame.textureY < 0 ||
                    frame.textureX + frame.textureWidth > atlas.getWidth() ||
                    frame.textureY + frame.textureHeight > atlas.getHeight()) {
                return null;
            }

            int[] pixels = new int[frame.textureWidth * frame.textureHeight];
            atlas.getPixels(
                    pixels,
                    0,
                    frame.textureWidth,
                    frame.textureX,
                    frame.textureY,
                    frame.textureWidth,
                    frame.textureHeight);
            int left = (frame.sourceWidth - frame.textureWidth) / 2 + frame.offsetX;
            int top = (frame.sourceHeight - frame.textureHeight) / 2 - frame.offsetY;
            return new SingleLoginAtlasComposer.AtlasLayer(
                    frame.textureWidth,
                    frame.textureHeight,
                    left,
                    top,
                    frame.sourceWidth,
                    frame.sourceHeight,
                    pixels);
        } finally {
            atlas.recycle();
        }
    }
}
