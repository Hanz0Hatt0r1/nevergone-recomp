package org.nevergone.recomp;

public final class GameSceneAtlasFramePixelsSmoke {
    private static GameSceneAtlasFrameResolver.Frame frame(
            int textureWidth,
            int textureHeight,
            int spriteWidth,
            int spriteHeight,
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
                new int[]{0, 0, textureWidth, textureHeight},
                new int[]{0, 0, spriteWidth, spriteHeight},
                new int[]{offsetX, offsetY},
                new int[]{spriteWidth, spriteHeight},
                new int[]{sourceWidth, sourceHeight},
                rotated);
    }

    private static GameSceneAtlasFrameResolver.Frame plainFrame(
            int textureWidth,
            int textureHeight,
            int spriteWidth,
            int spriteHeight,
            int sourceWidth,
            int sourceHeight,
            int offsetX,
            int offsetY) {
        return frame(
                textureWidth,
                textureHeight,
                spriteWidth,
                spriteHeight,
                sourceWidth,
                sourceHeight,
                offsetX,
                offsetY,
                false);
    }

    public static void main(String[] args) {
        // The format-3 loader uses spriteSize, not textureRect.size, for the
        // CCSpriteFrame rectangle. Prove that even deliberately inconsistent
        // exporter size metadata does not change the physical crop extent.
        GameSceneAtlasFrameResolver.Frame plain =
                plainFrame(99, 77, 2, 2, 4, 4, 0, 0);
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

        // A logical 3x2 rotated sprite is sampled from a physical 2x3 atlas
        // area because CCSprite::setTextureCoords swaps rect width/height for
        // rotated frames. The raw textureRect.size is not authoritative and is
        // intentionally inconsistent here.
        GameSceneAtlasFrameResolver.Frame rotated =
                frame(123, 456, 3, 2, 5, 4, 0, 0, true);
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

        GameSceneAtlasFrameResolver.Frame offset =
                plainFrame(0, 0, 2, 1, 6, 5, 1, -1);
        GameSceneAtlasFramePixels.Asset offsetAsset =
                GameSceneAtlasFramePixels.reconstruct(offset, 2, 1, new int[]{7, 8});
        assert offsetAsset != null;
        assert offsetAsset.pixels[21] == 7;
        assert offsetAsset.pixels[22] == 8;

        assert GameSceneAtlasFramePixels.reconstruct(rotated, 3, 2, new int[6]) == null;
        assert GameSceneAtlasFramePixels.reconstruct(plain, 2, 2, new int[3]) == null;

        GameSceneAtlasFrameResolver.Frame escaping =
                plainFrame(4, 4, 4, 4, 4, 4, 1, 0);
        assert GameSceneAtlasFramePixels.reconstruct(escaping, 4, 4, new int[16]) == null;
    }
}
