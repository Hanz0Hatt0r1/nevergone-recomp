package org.nevergone.recomp;

final class TexturePackerPixelTransform {
    static final class PixelBuffer {
        final int width;
        final int height;
        final int[] pixels;

        PixelBuffer(int width, int height, int[] pixels) {
            this.width = width;
            this.height = height;
            this.pixels = pixels;
        }
    }

    private TexturePackerPixelTransform() {}

    static PixelBuffer restoreUpright(
            int[] storedPixels, int storedWidth, int storedHeight, boolean rotated) {
        if (storedPixels == null || storedWidth <= 0 || storedHeight <= 0 ||
                storedPixels.length != storedWidth * storedHeight) {
            throw new IllegalArgumentException("invalid TexturePacker pixel buffer");
        }

        if (!rotated) {
            return new PixelBuffer(storedWidth, storedHeight, storedPixels);
        }

        // Cocos2d TexturePacker plists mark frames that were stored 90 degrees
        // clockwise in the atlas. Restore them counter-clockwise before upload.
        final int outputWidth = storedHeight;
        final int outputHeight = storedWidth;
        int[] output = new int[storedPixels.length];
        for (int y = 0; y < outputHeight; y++) {
            for (int x = 0; x < outputWidth; x++) {
                final int sourceX = storedWidth - 1 - y;
                final int sourceY = x;
                output[y * outputWidth + x] =
                        storedPixels[sourceY * storedWidth + sourceX];
            }
        }
        return new PixelBuffer(outputWidth, outputHeight, output);
    }
}
