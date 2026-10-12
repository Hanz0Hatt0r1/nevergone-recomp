package org.nevergone.recomp;

final class GameSceneDirectAssetStore {
    private GameSceneDirectAssetStore() {}

    private static native void nativeClear();
    private static native boolean nativeBegin(long revision, int expectedCount);
    private static native boolean nativeUpload(
            long revision,
            int requestIndex,
            int spriteCommandIndex,
            boolean rgba4444,
            boolean hasFrameOffset,
            int frameOffsetX,
            int frameOffsetY,
            int width,
            int height,
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
            int[] pixels) {
        return uploadInternal(
                revision,
                requestIndex,
                spriteCommandIndex,
                false,
                0,
                0,
                width,
                height,
                pixels);
    }

    static boolean uploadFrame(
            long revision,
            int requestIndex,
            int spriteCommandIndex,
            int frameOffsetX,
            int frameOffsetY,
            int width,
            int height,
            int[] pixels) {
        return uploadInternal(
                revision,
                requestIndex,
                spriteCommandIndex,
                true,
                frameOffsetX,
                frameOffsetY,
                width,
                height,
                pixels);
    }

    private static boolean uploadInternal(
            long revision,
            int requestIndex,
            int spriteCommandIndex,
            boolean hasFrameOffset,
            int frameOffsetX,
            int frameOffsetY,
            int width,
            int height,
            int[] pixels) {
        // MeActionsSystem::initWithFile sets CCTexture2D's default alpha pixel
        // format to recovered value 6 before loading the player action atlas.
        // The original format-6 conversion is RGBA4444. Keep this decision at
        // the request boundary so ordinary GameScene resources stay RGBA8888.
        boolean rgba4444 = GameSceneDirectAssetRequests.isPlayerBodyAt(requestIndex);
        return nativeUpload(
                revision,
                requestIndex,
                spriteCommandIndex,
                rgba4444,
                hasFrameOffset,
                frameOffsetX,
                frameOffsetY,
                width,
                height,
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
