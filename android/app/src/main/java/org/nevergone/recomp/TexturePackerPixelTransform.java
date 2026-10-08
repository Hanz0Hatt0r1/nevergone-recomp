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

    static final class Placement {
        final int left;
        final int top;

        Placement(int left, int top) {
            this.left = left;
            this.top = top;
        }
    }

    private TexturePackerPixelTransform() {}

    static PixelBuffer restoreUpright(
            int[] storedPixels, int storedWidth, int storedHeight, boolean rotated) {
        long expected = (long) storedWidth * (long) storedHeight;
        if (storedPixels == null || storedWidth <= 0 || storedHeight <= 0 ||
                expected != storedPixels.length) {
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

    static Placement placement(
            int sourceWidth,
            int sourceHeight,
            int uprightWidth,
            int uprightHeight,
            int offsetX,
            int offsetY) {
        if (sourceWidth <= 0 || sourceHeight <= 0 || uprightWidth <= 0 || uprightHeight <= 0 ||
                uprightWidth > sourceWidth || uprightHeight > sourceHeight) {
            throw new IllegalArgumentException("invalid TexturePacker frame geometry");
        }
        int left = (sourceWidth - uprightWidth) / 2 + offsetX;
        int top = (sourceHeight - uprightHeight) / 2 - offsetY;
        if (left < 0 || top < 0 || left + uprightWidth > sourceWidth ||
                top + uprightHeight > sourceHeight) {
            throw new IllegalArgumentException("TexturePacker trim placement outside source bounds");
        }
        return new Placement(left, top);
    }
}
