package org.nevergone.recomp;

import java.io.File;

/** Stages evidence-backed player action and spawn state once the offline scene is ready. */
final class PlayerActionRuntimeLoader {
    private static int attemptedCareer;
    private static String loadedRelativePath;
    private static int loadedActionState = -1;
    private static int failedActionState = -1;
    private static boolean loadedActionLooping;

    private PlayerActionRuntimeLoader() {}

    private static native boolean nativeIsOfflineSceneRenderQueueReady();
    private static native int nativeCurrentOfflineCareer();
    private static native boolean nativeRefreshPlayerSpawnState();
    private static native int nativeDesiredPlayerActionState();
    private static native boolean nativeCommitLoadedPlayerActionState(
            int actionState, boolean looping);
    private static native boolean nativeLoadPlayerActionDocument(String relativePath);
    private static native boolean nativeLoadPlayerActionDocumentFromBundle(
            String bundleRelativePath,
            String entryName);
    private static native void nativeResetPlayerActionDocument();
    private static native String nativeStatusReport();

    static void invalidate() {
        attemptedCareer = 0;
        loadedRelativePath = null;
        loadedActionState = -1;
        failedActionState = -1;
        loadedActionLooping = false;
        nativeResetPlayerActionDocument();
    }

    static boolean stageIfNeeded(File assetRoot) {
        if (!nativeIsOfflineSceneRenderQueueReady()) {
            if (attemptedCareer != 0 || loadedRelativePath != null || loadedActionState >= 0) {
                invalidate();
            }
            return false;
        }

        int career = nativeCurrentOfflineCareer();
        if (career < 1 || career > 5) {
            if (attemptedCareer != 0 || loadedRelativePath != null || loadedActionState >= 0) {
                invalidate();
            }
            return false;
        }

        if (career != attemptedCareer) {
            attemptedCareer = career;
            loadedRelativePath = null;
            loadedActionState = -1;
            failedActionState = -1;
            loadedActionLooping = false;
            nativeResetPlayerActionDocument();
        }

        // Refresh after a career change: resetting the action document also
        // resets spawn/motion state, and save restoration must see that spawn.
        // Refresh every frame to retain the current scene/port/surface marker.
        boolean spawnReady = nativeRefreshPlayerSpawnState();

        int desiredActionState = nativeDesiredPlayerActionState();
        if (desiredActionState < 0) return false;
        if (desiredActionState == loadedActionState) {
            return spawnReady && loadedRelativePath != null;
        }
        // A missing/corrupt imported action must not cause recursive plist and
        // bundle scans every rendered frame. Explicit asset invalidation clears
        // this sentinel and permits a retry after the user replaces files.
        if (desiredActionState == failedActionState) return false;

        PlayerActionAssetResolver.Result resolved =
                PlayerActionAssetResolver.resolveState(assetRoot, career, desiredActionState);
        if (resolved == null) {
            failedActionState = desiredActionState;
            return false;
        }

        boolean loaded;
        String retainedPath;
        if (resolved.isEmbedded()) {
            loaded = nativeLoadPlayerActionDocumentFromBundle(
                    resolved.actionBundleRelativePath,
                    resolved.actionBundleEntryName);
            retainedPath = resolved.actionBundleRelativePath;
        } else {
            if (resolved.actionDataRelativePath == null ||
                    resolved.actionDataRelativePath.isEmpty()) {
                failedActionState = desiredActionState;
                return false;
            }
            loaded = nativeLoadPlayerActionDocument(resolved.actionDataRelativePath);
            retainedPath = resolved.actionDataRelativePath;
        }
        if (!loaded || !nativeCommitLoadedPlayerActionState(
                desiredActionState, resolved.looping)) {
            failedActionState = desiredActionState;
            return false;
        }

        loadedRelativePath = retainedPath;
        loadedActionState = desiredActionState;
        loadedActionLooping = resolved.looping;
        failedActionState = -1;
        return spawnReady;
    }

    static String statusReport() {
        String nativeReport = nativeStatusReport();
        StringBuilder out = new StringBuilder();
        if (nativeReport != null && !nativeReport.isEmpty()) out.append(nativeReport);
        out.append("loader attempted career: ").append(attemptedCareer).append('\n');
        out.append("loader action state: ").append(loadedActionState).append('\n');
        out.append("loader failed action state: ").append(failedActionState).append('\n');
        out.append("loader action looping: ").append(loadedActionLooping).append('\n');
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

    static int loadedActionStateForTest() {
        return loadedActionState;
    }

    static int loadedCareer() {
        return loadedRelativePath != null ? attemptedCareer : 0;
    }
}
