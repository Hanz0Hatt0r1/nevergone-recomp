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
        if (frame == null) return 0;
        // textureRect is the physical rectangle stored in the atlas. For a
        // rotated TexturePacker frame its dimensions are already swapped with
        // respect to spriteSize, so do not swap them a second time here.
        return frame.textureWidth;
    }

    static int packedHeight(GameSceneAtlasFrameResolver.Frame frame) {
        if (frame == null) return 0;
        return frame.textureHeight;
    }

    static Asset reconstruct(
            GameSceneAtlasFrameResolver.Frame frame,
            int packedWidth,
            int packedHeight,
            int[] packedPixels) {
        if (frame == null || packedPixels == null ||
                frame.textureWidth <= 0 || frame.textureHeight <= 0 ||
                frame.spriteWidth <= 0 || frame.spriteHeight <= 0 ||
                frame.sourceWidth <= 0 || frame.sourceHeight <= 0) {
            return null;
        }

        // TexturePacker format-3 writes textureRect in packed-atlas
        // orientation while spriteSize remains in the logical, unrotated
        // orientation. A rotated frame therefore has swapped dimensions.
        if (frame.rotated) {
            if (frame.textureWidth != frame.spriteHeight ||
                    frame.textureHeight != frame.spriteWidth) {
                return null;
            }
        } else if (frame.textureWidth != frame.spriteWidth ||
                frame.textureHeight != frame.spriteHeight) {
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
                    // TexturePacker's cocos2d exporter stores a clockwise-
                    // rotated logical sprite. For logical (x,y), the packed
                    // atlas coordinate is (logicalHeight - 1 - y, x).
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
