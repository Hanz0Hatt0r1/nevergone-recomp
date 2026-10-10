package org.nevergone.recomp;

public final class GameSceneAtlasFramePixelsSmoke {
    private static GameSceneAtlasFrameResolver.Frame frame(
            int width,
            int height,
            int sourceWidth,
            int sourceHeight,
            int offsetX,
            int offsetY,
            boolean rotated) {
        GameSceneAtlasResolver.Atlas atlas =
                new GameSceneAtlasResolver.Atlas("gamescene/sheet.plist", "gamescene/sheet.png");
        return new GameSceneAtlasFrameResolver.Frame(
                0,
                "frame.png",
                "frame.png",
                atlas,
                new int[]{0, 0, width, height},
                new int[]{0, 0, width, height},
                new int[]{offsetX, offsetY},
                new int[]{width, height},
                new int[]{sourceWidth, sourceHeight},
                rotated);
    }

    public static void main(String[] args) {
        GameSceneAtlasFrameResolver.Frame plain = frame(2, 2, 4, 4, 0, 0, false);
        assert GameSceneAtlasFramePixels.packedWidth(plain) == 2;
        assert GameSceneAtlasFramePixels.packedHeight(plain) == 2;
        GameSceneAtlasFramePixels.Asset plainAsset =
                GameSceneAtlasFramePixels.reconstruct(plain, 2, 2, new int[]{1, 2, 3, 4});
        assert plainAsset != null;
        assert plainAsset.width == 4 && plainAsset.height == 4;
        assert plainAsset.pixels.length == 16;
        assert plainAsset.pixels[5] == 1;
        assert plainAsset.pixels[6] == 2;
        assert plainAsset.pixels[9] == 3;
        assert plainAsset.pixels[10] == 4;
        assert plainAsset.pixels[0] == 0;

        // Logical trim is 3x2. TexturePacker stores a clockwise-rotated frame
        // physically as 2x3: [4 1] [5 2] [6 3]. Reconstruction restores the
        // unrotated 3x2 sprite before placing it in the 5x4 source rectangle.
        GameSceneAtlasFrameResolver.Frame rotated = frame(3, 2, 5, 4, 0, 0, true);
        assert GameSceneAtlasFramePixels.packedWidth(rotated) == 2;
        assert GameSceneAtlasFramePixels.packedHeight(rotated) == 3;
        GameSceneAtlasFramePixels.Asset rotatedAsset =
                GameSceneAtlasFramePixels.reconstruct(
                        rotated, 2, 3, new int[]{4, 1, 5, 2, 6, 3});
        assert rotatedAsset != null;
        assert rotatedAsset.width == 5 && rotatedAsset.height == 4;
        assert rotatedAsset.pixels[6] == 1;
        assert rotatedAsset.pixels[7] == 2;
        assert rotatedAsset.pixels[8] == 3;
        assert rotatedAsset.pixels[11] == 4;
        assert rotatedAsset.pixels[12] == 5;
        assert rotatedAsset.pixels[13] == 6;

        GameSceneAtlasFrameResolver.Frame offset = frame(2, 1, 6, 5, 1, -1, false);
        GameSceneAtlasFramePixels.Asset offsetAsset =
                GameSceneAtlasFramePixels.reconstruct(offset, 2, 1, new int[]{7, 8});
        assert offsetAsset != null;
        assert offsetAsset.pixels[21] == 7;
        assert offsetAsset.pixels[22] == 8;

        assert GameSceneAtlasFramePixels.reconstruct(rotated, 3, 2, new int[6]) == null;
        assert GameSceneAtlasFramePixels.reconstruct(plain, 2, 2, new int[3]) == null;

        GameSceneAtlasFrameResolver.Frame escaping = frame(4, 4, 4, 4, 1, 0, false);
        assert GameSceneAtlasFramePixels.reconstruct(escaping, 4, 4, new int[16]) == null;
    }
}
