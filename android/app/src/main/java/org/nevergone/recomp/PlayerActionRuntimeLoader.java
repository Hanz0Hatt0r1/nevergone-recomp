package org.nevergone.recomp;

import java.io.File;

/** Stages the evidence-backed initial player action document once the offline scene is ready. */
final class PlayerActionRuntimeLoader {
    private static int attemptedCareer;
    private static String loadedRelativePath;

    private PlayerActionRuntimeLoader() {}

    private static native boolean nativeIsOfflineSceneRenderQueueReady();
    private static native int nativeCurrentOfflineCareer();
    private static native boolean nativeLoadPlayerActionDocument(String relativePath);
    private static native void nativeResetPlayerActionDocument();

    static void invalidate() {
        attemptedCareer = 0;
        loadedRelativePath = null;
        nativeResetPlayerActionDocument();
    }

    static boolean stageIfNeeded(File assetRoot) {
        if (!nativeIsOfflineSceneRenderQueueReady()) {
            if (attemptedCareer != 0 || loadedRelativePath != null) invalidate();
            return false;
        }

        int career = nativeCurrentOfflineCareer();
        if (career < 1 || career > 5) {
            if (attemptedCareer != 0 || loadedRelativePath != null) invalidate();
            return false;
        }
        if (career == attemptedCareer) return loadedRelativePath != null;

        attemptedCareer = career;
        loadedRelativePath = null;
        nativeResetPlayerActionDocument();
        PlayerActionAssetResolver.Result resolved =
                PlayerActionAssetResolver.resolveInitial(assetRoot, career);
        if (resolved == null || resolved.actionDataRelativePath == null ||
                resolved.actionDataRelativePath.isEmpty()) {
            return false;
        }
        if (!nativeLoadPlayerActionDocument(resolved.actionDataRelativePath)) return false;
        loadedRelativePath = resolved.actionDataRelativePath;
        return true;
    }

    static int attemptedCareerForTest() {
        return attemptedCareer;
    }

    static String loadedRelativePathForTest() {
        return loadedRelativePath;
    }
}
