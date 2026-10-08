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

        TexturePackerPixelTransform.Placement full =
                TexturePackerPixelTransform.placement(1137, 162, 1137, 162, 0, 0);
        if (full.left != 0 || full.top != 0) {
            throw new AssertionError("full-size vine placement changed");
        }

        // Recovered xrtengman03 metadata: source 223x482, trimmed 219x482,
        // spriteOffset x=1. TexturePacker's spriteSourceSize starts at x=3.
        TexturePackerPixelTransform.Placement trimmed =
                TexturePackerPixelTransform.placement(223, 482, 219, 482, 1, 0);
        if (trimmed.left != 3 || trimmed.top != 0) {
            throw new AssertionError("trimmed vine placement mismatch");
        }

        boolean rejected = false;
        try {
            TexturePackerPixelTransform.restoreUpright(new int[] {1, 2, 3}, 2, 2, true);
        } catch (IllegalArgumentException expectedFailure) {
            rejected = true;
        }
        if (!rejected) throw new AssertionError("invalid pixel buffer accepted");

        rejected = false;
        try {
            TexturePackerPixelTransform.placement(10, 10, 9, 9, 10, 0);
        } catch (IllegalArgumentException expectedFailure) {
            rejected = true;
        }
        if (!rejected) throw new AssertionError("out-of-bounds trim placement accepted");

        System.out.println("TexturePacker rotated-frame transform and placement: ok");
    }
}
