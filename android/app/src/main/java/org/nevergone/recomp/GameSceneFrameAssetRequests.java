package org.nevergone.recomp;

final class GameSceneFrameAssetRequests {
    private GameSceneFrameAssetRequests() {}

    private static native long nativeRefresh();
    private static native int nativeCount();
    private static native String nativeFrameNameAt(int requestIndex);
    private static native int nativeSpriteCommandIndexAt(int requestIndex);

    // refresh() atomically captures the current native frame-cache request
    // snapshot. The indexed accessors read that same snapshot until the next
    // refresh so one staging pass cannot mix two GameScene queues.
    static long refresh() {
        return nativeRefresh();
    }

    static int count() {
        return nativeCount();
    }

    static String frameNameAt(int requestIndex) {
        return nativeFrameNameAt(requestIndex);
    }

    static int spriteCommandIndexAt(int requestIndex) {
        return nativeSpriteCommandIndexAt(requestIndex);
    }
}
