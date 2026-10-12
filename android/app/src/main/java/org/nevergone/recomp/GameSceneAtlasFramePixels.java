package org.nevergone.recomp;

/** Reconstructs TexturePacker frame pixels in logical sprite orientation. */
final class GameSceneAtlasFramePixels {
    private static final long MAX_SOURCE_PIXELS = 16L * 1024L * 1024L;

    static final class Asset {
        final int width;
        final int height;
        final int[] pixels;

        Asset(int width, int height, int[] pixels) {
            this.width = width;
            this.height = height;
            this.pixels = pixels;
        }
    }

    private GameSceneAtlasFramePixels() {}

    static int packedWidth(GameSceneAtlasFrameResolver.Frame frame) {
        if (frame == null || frame.spriteWidth <= 0 || frame.spriteHeight <= 0) return 0;
        // cocos2d-x 2.1.2 format-3 ignores textureRect.size entirely. It
        // constructs the CCSpriteFrame rect from textureRect.origin plus
        // spriteSize, and CCSprite::setTextureCoords swaps the sampled width/
        // height for a rotated frame. Mirror that final physical atlas extent.
        return frame.rotated ? frame.spriteHeight : frame.spriteWidth;
    }

    static int packedHeight(GameSceneAtlasFrameResolver.Frame frame) {
        if (frame == null || frame.spriteWidth <= 0 || frame.spriteHeight <= 0) return 0;
        return frame.rotated ? frame.spriteWidth : frame.spriteHeight;
    }

    /**
     * Return only the real trimmed frame pixels, rotated back into logical
     * sprite orientation. CCSpriteFrame keeps source-size/offset separately;
     * callers that retain those fields do not need a transparent source-size
     * bitmap around every frame.
     */
    static Asset extractTrimmed(
            GameSceneAtlasFrameResolver.Frame frame,
            int packedWidth,
            int packedHeight,
            int[] packedPixels) {
        if (frame == null || packedPixels == null ||
                frame.spriteWidth <= 0 || frame.spriteHeight <= 0) {
            return null;
        }

        final int expectedPackedWidth = packedWidth(frame);
        final int expectedPackedHeight = packedHeight(frame);
        final int logicalWidth = frame.spriteWidth;
        final int logicalHeight = frame.spriteHeight;
        final long packedCount = (long) expectedPackedWidth * (long) expectedPackedHeight;
        final long logicalCount = (long) logicalWidth * (long) logicalHeight;
        if (expectedPackedWidth <= 0 || expectedPackedHeight <= 0 ||
                packedWidth != expectedPackedWidth || packedHeight != expectedPackedHeight ||
                packedCount <= 0L || packedCount > Integer.MAX_VALUE ||
                packedPixels.length != (int) packedCount ||
                logicalCount <= 0L || logicalCount > MAX_SOURCE_PIXELS ||
                logicalCount > Integer.MAX_VALUE) {
            return null;
        }

        if (!frame.rotated) {
            int[] output = new int[(int) logicalCount];
            System.arraycopy(packedPixels, 0, output, 0, output.length);
            return new Asset(logicalWidth, logicalHeight, output);
        }

        int[] output = new int[(int) logicalCount];
        for (int y = 0; y < logicalHeight; ++y) {
            for (int x = 0; x < logicalWidth; ++x) {
                // Pixel equivalent of the original rotated UV mapping:
                // physical atlas extent HxW -> logical standalone WxH.
                final int packedX = logicalHeight - 1 - y;
                final int packedY = x;
                final int packedIndex = packedY * expectedPackedWidth + packedX;
                output[y * logicalWidth + x] = packedPixels[packedIndex];
            }
        }
        return new Asset(logicalWidth, logicalHeight, output);
    }

    /**
     * Compatibility helper for callers/tests that still need a standalone
     * untrimmed bitmap. The live scene staging path now retains trim metadata
     * instead and uploads extractTrimmed() directly.
     */
    static Asset reconstruct(
            GameSceneAtlasFrameResolver.Frame frame,
            int packedWidth,
            int packedHeight,
            int[] packedPixels) {
        if (frame == null || frame.sourceWidth <= 0 || frame.sourceHeight <= 0) return null;
        Asset trimmed = extractTrimmed(frame, packedWidth, packedHeight, packedPixels);
        if (trimmed == null) return null;

        final long sourceCount = (long) frame.sourceWidth * (long) frame.sourceHeight;
        if (sourceCount <= 0L || sourceCount > MAX_SOURCE_PIXELS ||
                sourceCount > Integer.MAX_VALUE) {
            return null;
        }

        // CCSpriteFrame stores spriteOffset as the displacement from the
        // untrimmed sprite center to the trimmed sprite center. Android bitmap
        // rows grow downward, so positive Cocos Y moves the trim upward.
        final long leftLong =
                ((long) frame.sourceWidth - (long) trimmed.width) / 2L + frame.offsetX;
        final long topLong =
                ((long) frame.sourceHeight - (long) trimmed.height) / 2L - frame.offsetY;
        if (leftLong < 0L || topLong < 0L ||
                leftLong + trimmed.width > frame.sourceWidth ||
                topLong + trimmed.height > frame.sourceHeight) {
            return null;
        }
        final int left = (int) leftLong;
        final int top = (int) topLong;

        int[] output = new int[(int) sourceCount];
        for (int y = 0; y < trimmed.height; ++y) {
            System.arraycopy(
                    trimmed.pixels,
                    y * trimmed.width,
                    output,
                    (top + y) * frame.sourceWidth + left,
                    trimmed.width);
        }
        return new Asset(frame.sourceWidth, frame.sourceHeight, output);
    }
}
