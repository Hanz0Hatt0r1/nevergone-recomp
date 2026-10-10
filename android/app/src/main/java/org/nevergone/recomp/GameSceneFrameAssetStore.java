package org.nevergone.recomp;

final class GameSceneFrameAssetStore {
    private GameSceneFrameAssetStore() {}

    private static native void nativeClear();
    private static native boolean nativeBegin(long revision, int expectedCount);
    private static native boolean nativeUpload(
            long revision,
            int requestIndex,
            int spriteCommandIndex,
            int width,
            int height,
            int sourceWidth,
            int sourceHeight,
            int left,
            int top,
            int[] pixels);
    private static native boolean nativeFinish(long revision);
    private static native void nativeCancel(long revision);
    private static native long nativeActiveRevision();
    private static native int nativeActiveAssetCount();

    static void clear() {
        nativeClear();
    }

    static boolean begin(long revision, int expectedCount) {
        return nativeBegin(revision, expectedCount);
    }

    static boolean upload(
            long revision,
            int requestIndex,
            int spriteCommandIndex,
            int width,
            int height,
            int sourceWidth,
            int sourceHeight,
            int left,
            int top,
            int[] pixels) {
        return nativeUpload(
                revision,
                requestIndex,
                spriteCommandIndex,
                width,
                height,
                sourceWidth,
                sourceHeight,
                left,
                top,
                pixels);
    }

    static boolean finish(long revision) {
        return nativeFinish(revision);
    }

    static void cancel(long revision) {
        nativeCancel(revision);
    }

    static long activeRevision() {
        return nativeActiveRevision();
    }

    static int activeAssetCount() {
        return nativeActiveAssetCount();
    }
}
