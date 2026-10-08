package org.nevergone.recomp;

import android.graphics.Bitmap;

final class TexturePackerAtlasExtractor {
    static final class ExtractedFrame {
        final int width;
        final int height;
        final int left;
        final int top;
        final int sourceWidth;
        final int sourceHeight;
        final int[] pixels;

        ExtractedFrame(
                int width,
                int height,
                int left,
                int top,
                int sourceWidth,
                int sourceHeight,
                int[] pixels) {
            this.width = width;
            this.height = height;
            this.left = left;
            this.top = top;
            this.sourceWidth = sourceWidth;
            this.sourceHeight = sourceHeight;
            this.pixels = pixels;
        }
    }

    private TexturePackerAtlasExtractor() {}

    static ExtractedFrame extract(TexturePackerPlist.Frame frame, Bitmap atlas) {
        if (frame == null || atlas == null ||
                frame.textureWidth <= 0 || frame.textureHeight <= 0 ||
                frame.sourceWidth <= 0 || frame.sourceHeight <= 0 ||
                ((long) frame.textureWidth * (long) frame.textureHeight) > 16_777_216L ||
                ((long) frame.sourceWidth * (long) frame.sourceHeight) > 16_777_216L ||
                frame.textureX < 0 || frame.textureY < 0 ||
                frame.textureX + frame.textureWidth > atlas.getWidth() ||
                frame.textureY + frame.textureHeight > atlas.getHeight()) {
            return null;
        }

        int[] storedPixels = new int[frame.textureWidth * frame.textureHeight];
        atlas.getPixels(
                storedPixels,
                0,
                frame.textureWidth,
                frame.textureX,
                frame.textureY,
                frame.textureWidth,
                frame.textureHeight);

        try {
            TexturePackerPixelTransform.PixelBuffer upright =
                    TexturePackerPixelTransform.restoreUpright(
                            storedPixels,
                            frame.textureWidth,
                            frame.textureHeight,
                            frame.rotated);
            TexturePackerPixelTransform.Placement placement =
                    TexturePackerPixelTransform.placement(
                            frame.sourceWidth,
                            frame.sourceHeight,
                            upright.width,
                            upright.height,
                            frame.offsetX,
                            frame.offsetY);
            return new ExtractedFrame(
                    upright.width,
                    upright.height,
                    placement.left,
                    placement.top,
                    frame.sourceWidth,
                    frame.sourceHeight,
                    upright.pixels);
        } catch (IllegalArgumentException ignored) {
            return null;
        }
    }
}
