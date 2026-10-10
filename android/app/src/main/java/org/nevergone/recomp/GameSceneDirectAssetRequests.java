package org.nevergone.recomp;

final class GameSceneDirectAssetRequests {
    private GameSceneDirectAssetRequests() {}

    private static native long nativeRefresh();
    private static native int nativeCount();
    private static native String nativePathAt(int requestIndex);
    private static native int nativeSpriteCommandIndexAt(int requestIndex);

    // Refresh atomically captures the current native request snapshot. Count,
    // pathAt and spriteCommandIndexAt read that same snapshot until refresh is
    // called again, so one GL-frame staging pass cannot mix two scene queues.
    static long refresh() {
        return nativeRefresh();
    }

    static int count() {
        return nativeCount();
    }

    static String pathAt(int requestIndex) {
        return nativePathAt(requestIndex);
    }

    static int spriteCommandIndexAt(int requestIndex) {
        return nativeSpriteCommandIndexAt(requestIndex);
    }
}
