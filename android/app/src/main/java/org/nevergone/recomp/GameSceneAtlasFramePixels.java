package org.nevergone.recomp;

/** Reconstructs one TexturePacker frame as a standalone untrimmed ARGB sprite. */
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

    static Asset reconstruct(
            GameSceneAtlasFrameResolver.Frame frame,
            int packedWidth,
            int packedHeight,
            int[] packedPixels) {
        if (frame == null || packedPixels == null ||
                frame.spriteWidth <= 0 || frame.spriteHeight <= 0 ||
                frame.sourceWidth <= 0 || frame.sourceHeight <= 0) {
            return null;
        }

        final int expectedPackedWidth = packedWidth(frame);
        final int expectedPackedHeight = packedHeight(frame);
        final int logicalWidth = frame.spriteWidth;
        final int logicalHeight = frame.spriteHeight;
        final long packedCount = (long) expectedPackedWidth * (long) expectedPackedHeight;
        final long sourceCount = (long) frame.sourceWidth * (long) frame.sourceHeight;
        if (expectedPackedWidth <= 0 || expectedPackedHeight <= 0 ||
                packedWidth != expectedPackedWidth || packedHeight != expectedPackedHeight ||
                packedCount <= 0L || packedCount > Integer.MAX_VALUE ||
                packedPixels.length != (int) packedCount ||
                sourceCount <= 0L || sourceCount > MAX_SOURCE_PIXELS ||
                sourceCount > Integer.MAX_VALUE) {
            return null;
        }

        // CCSpriteFrame format-3 stores spriteOffset as the displacement from
        // the untrimmed sprite center to the trimmed sprite center. Android
        // bitmap rows grow downward, so positive Cocos Y moves the trim upward.
        final long leftLong =
                ((long) frame.sourceWidth - (long) logicalWidth) / 2L + frame.offsetX;
        final long topLong =
                ((long) frame.sourceHeight - (long) logicalHeight) / 2L - frame.offsetY;
        if (leftLong < 0L || topLong < 0L ||
                leftLong + logicalWidth > frame.sourceWidth ||
                topLong + logicalHeight > frame.sourceHeight) {
            return null;
        }
        final int left = (int) leftLong;
        final int top = (int) topLong;

        int[] output = new int[(int) sourceCount];
        for (int y = 0; y < logicalHeight; ++y) {
            for (int x = 0; x < logicalWidth; ++x) {
                final int packedIndex;
                if (frame.rotated) {
                    // This is the pixel equivalent of the original rotated UV
                    // mapping: physical atlas extent HxW, restored clockwise-
                    // packed data into a logical WxH sprite.
                    final int packedX = logicalHeight - 1 - y;
                    final int packedY = x;
                    packedIndex = packedY * expectedPackedWidth + packedX;
                } else {
                    packedIndex = y * expectedPackedWidth + x;
                }
                final int outputIndex = (top + y) * frame.sourceWidth + left + x;
                output[outputIndex] = packedPixels[packedIndex];
            }
        }
        return new Asset(frame.sourceWidth, frame.sourceHeight, output);
    }
}
