package org.nevergone.recomp;

import java.io.File;

/** Stages evidence-backed player action and spawn state once the offline scene is ready. */
final class PlayerActionRuntimeLoader {
    private static int attemptedCareer;
    private static String loadedRelativePath;

    private PlayerActionRuntimeLoader() {}

    private static native boolean nativeIsOfflineSceneRenderQueueReady();
    private static native int nativeCurrentOfflineCareer();
    private static native boolean nativeRefreshPlayerSpawnState();
    private static native boolean nativeLoadPlayerActionDocument(String relativePath);
    private static native void nativeResetPlayerActionDocument();
    private static native String nativeStatusReport();

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

        // Refresh from the current scene/event-port/surface every frame. The
        // operation is bounded to the current parsed scene and preserves the
        // original ordered overwrite behavior for multiple matching markers.
        boolean spawnReady = nativeRefreshPlayerSpawnState();

        int career = nativeCurrentOfflineCareer();
        if (career < 1 || career > 5) {
            if (attemptedCareer != 0 || loadedRelativePath != null) invalidate();
            return false;
        }
        if (career == attemptedCareer) return spawnReady && loadedRelativePath != null;

        attemptedCareer = career;
        loadedRelativePath = null;
        nativeResetPlayerActionDocument();
        // reset also clears spawn state, so restore it immediately for this
        // same live scene before loading the action document.
        spawnReady = nativeRefreshPlayerSpawnState();
        PlayerActionAssetResolver.Result resolved =
                PlayerActionAssetResolver.resolveInitial(assetRoot, career);
        if (resolved == null || resolved.actionDataRelativePath == null ||
                resolved.actionDataRelativePath.isEmpty()) {
            return false;
        }
        if (!nativeLoadPlayerActionDocument(resolved.actionDataRelativePath)) return false;
        loadedRelativePath = resolved.actionDataRelativePath;
        return spawnReady;
    }

    static String statusReport() {
        String nativeReport = nativeStatusReport();
        StringBuilder out = new StringBuilder();
        if (nativeReport != null && !nativeReport.isEmpty()) out.append(nativeReport);
        out.append("loader attempted career: ").append(attemptedCareer).append('\n');
        out.append("loader document retained: ")
                .append(loadedRelativePath != null ? "yes" : "no")
                .append('\n');
        return out.toString();
    }

    static int attemptedCareerForTest() {
        return attemptedCareer;
    }

    static String loadedRelativePathForTest() {
        return loadedRelativePath;
    }

    static int loadedCareer() {
        return loadedRelativePath != null ? attemptedCareer : 0;
    }
}
