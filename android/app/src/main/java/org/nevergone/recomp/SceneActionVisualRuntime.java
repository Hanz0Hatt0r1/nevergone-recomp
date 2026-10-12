package org.nevergone.recomp;

import java.io.File;

/** Transactional staging coordinator for one explicitly selected klhuo SceneActions variant. */
final class SceneActionVisualRuntime {
    private static int readyVariant;

    private SceneActionVisualRuntime() {}

    static void invalidate() {
        readyVariant = 0;
        // Pair reset also clears the native initial-frame pixel store.
        SceneActionPairRuntimeLoader.invalidate();
    }

    static boolean stageVariant(File assetRoot, int variant) {
        if (assetRoot == null || variant < 1 || variant > 4) return false;
        if (ready(variant)) return true;

        if (!SceneActionPairRuntimeLoader.stageVariant(assetRoot, variant)) {
            invalidate();
            return false;
        }
        if (!SceneActionInitialFramePixelStager.stage(
                assetRoot, SceneActionPairRuntimeLoader.SLOT_BORN, variant)) {
            invalidate();
            return false;
        }
        if (!SceneActionInitialFramePixelStager.stage(
                assetRoot, SceneActionPairRuntimeLoader.SLOT_IDLE, variant)) {
            invalidate();
            return false;
        }
        if (!SceneActionInitialFramePixelStager.ready(
                    SceneActionPairRuntimeLoader.SLOT_BORN, variant) ||
                !SceneActionInitialFramePixelStager.ready(
                    SceneActionPairRuntimeLoader.SLOT_IDLE, variant)) {
            invalidate();
            return false;
        }

        readyVariant = variant;
        return true;
    }

    static boolean ready(int variant) {
        if (variant < 1 || variant > 4 || readyVariant != variant ||
                SceneActionPairRuntimeLoader.loadedVariantForTest() != variant) {
            return false;
        }
        return SceneActionInitialFramePixelStager.ready(
                        SceneActionPairRuntimeLoader.SLOT_BORN, variant) &&
                SceneActionInitialFramePixelStager.ready(
                        SceneActionPairRuntimeLoader.SLOT_IDLE, variant);
    }

    static int readyVariantForTest() {
        return readyVariant;
    }
}
