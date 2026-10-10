package org.nevergone.recomp;

final class GameSceneDirectAssetRequests {
    private GameSceneDirectAssetRequests() {}

    private static native long nativeRevision();
    private static native int nativeCount();
    private static native String nativePathAt(int requestIndex);
    private static native int nativeSpriteCommandIndexAt(int requestIndex);

    static long revision() {
        return nativeRevision();
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
