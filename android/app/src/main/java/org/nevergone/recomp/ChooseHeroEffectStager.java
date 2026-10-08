package org.nevergone.recomp;

final class ChooseHeroEffectStager {
    private static final int EFFECT_FRAME_COUNT = 6;
    private static final int LIGHTNING_FIRST_INDEX = 10;
    private static final int THUNDER_FIRST_INDEX = 16;

    private ChooseHeroEffectStager() {}

    private static native boolean nativeUploadEffectAsset(
            int frameIndex,
            int width,
            int height,
            int left,
            int top,
            int sourceWidth,
            int sourceHeight,
            int[] argbPixels);

    static native boolean nativeEffectAssetsReady();

    static boolean stage(
            TexturePackerAtlasExtractor.ExtractedFrame[] lightning,
            TexturePackerAtlasExtractor.ExtractedFrame[] thunder) {
        if (lightning == null || thunder == null ||
                lightning.length != EFFECT_FRAME_COUNT ||
                thunder.length != EFFECT_FRAME_COUNT) {
            return false;
        }

        for (int index = 0; index < EFFECT_FRAME_COUNT; index++) {
            if (!upload(LIGHTNING_FIRST_INDEX + index, lightning[index])) return false;
        }
        for (int index = 0; index < EFFECT_FRAME_COUNT; index++) {
            if (!upload(THUNDER_FIRST_INDEX + index, thunder[index])) return false;
        }
        return nativeEffectAssetsReady();
    }

    private static boolean upload(
            int frameIndex,
            TexturePackerAtlasExtractor.ExtractedFrame frame) {
        return frame != null && nativeUploadEffectAsset(
                frameIndex,
                frame.width,
                frame.height,
                frame.left,
                frame.top,
                frame.sourceWidth,
                frame.sourceHeight,
                frame.pixels);
    }
}
