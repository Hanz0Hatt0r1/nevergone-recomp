package org.nevergone.recomp;

final class GameSceneDirectAssetRequests {
    static final int KIND_DIRECT_FILE = 0;
    static final int KIND_SPRITE_FRAME_BY_NAME = 1;

    private GameSceneDirectAssetRequests() {}

    private static native long nativeRefresh();
    private static native int nativeCount();
    private static native int nativeKindAt(int requestIndex);
    private static native String nativeValueAt(int requestIndex);
    private static native String nativePathAt(int requestIndex);
    private static native int nativeSpriteCommandIndexAt(int requestIndex);

    // Refresh atomically captures the current native request snapshot. Count,
    // kind/value and spriteCommandIndexAt read that same snapshot until refresh
    // is called again, so one GL-frame staging pass cannot mix two scene queues.
    static long refresh() {
        return nativeRefresh();
    }

    static int count() {
        return nativeCount();
    }

    static int kindAt(int requestIndex) {
        return nativeKindAt(requestIndex);
    }

    static String valueAt(int requestIndex) {
        return nativeValueAt(requestIndex);
    }

    static String pathAt(int requestIndex) {
        return nativePathAt(requestIndex);
    }

    static int spriteCommandIndexAt(int requestIndex) {
        return nativeSpriteCommandIndexAt(requestIndex);
    }
}
