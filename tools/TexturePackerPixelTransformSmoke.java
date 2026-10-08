package org.nevergone.recomp;

import java.util.Arrays;

public final class TexturePackerPixelTransformSmoke {
    public static void main(String[] args) {
        int[] plain = {1, 2, 3, 4};
        TexturePackerPixelTransform.PixelBuffer unchanged =
                TexturePackerPixelTransform.restoreUpright(plain, 2, 2, false);
        if (unchanged.width != 2 || unchanged.height != 2 ||
                !Arrays.equals(unchanged.pixels, plain)) {
            throw new AssertionError("non-rotated frame changed");
        }

        // Upright 2x3 frame:
        // 1 2
        // 3 4
        // 5 6
        // TexturePacker stores the clockwise rotation as a 3x2 frame:
        // 5 3 1
        // 6 4 2
        int[] clockwiseStored = {5, 3, 1, 6, 4, 2};
        TexturePackerPixelTransform.PixelBuffer restored =
                TexturePackerPixelTransform.restoreUpright(clockwiseStored, 3, 2, true);
        int[] expected = {1, 2, 3, 4, 5, 6};
        if (restored.width != 2 || restored.height != 3 ||
                !Arrays.equals(restored.pixels, expected)) {
            throw new AssertionError("rotated TexturePacker frame was not restored");
        }

        boolean rejected = false;
        try {
            TexturePackerPixelTransform.restoreUpright(new int[] {1, 2, 3}, 2, 2, true);
        } catch (IllegalArgumentException expectedFailure) {
            rejected = true;
        }
        if (!rejected) throw new AssertionError("invalid pixel buffer accepted");

        System.out.println("TexturePacker rotated-frame transform: ok");
    }
}
