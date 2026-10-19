package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import java.io.File;

final class GameSceneActionAssetLoader {
    private static final String[][] RESOURCE_NAMES = {
            {"ab.png", "ab_s.png"},
            {"db.png", "db_s.png"},
            {"jb.png", "jb_s.png"}
    };
    private static final String[] QUICK_USE_RESOURCE_NAMES = {
            "stage_use_a.png", "stage_use_b.png"
    };
    private static final long MAX_PIXELS = 16L * 1024L * 1024L;

    private GameSceneActionAssetLoader() {}

    static boolean reloadFromFilesDir(String filesDir) {
        if (filesDir == null || filesDir.isEmpty()) return false;
        GameSceneControlSettingsLoader.reloadFromFilesDir(filesDir);
        File assetRoot = new File(filesDir, "assets");
        if (!assetRoot.isDirectory()) return false;

        nativeClearBounds();
        nativeClearQuickUseBounds();
        nativeClearSprites();

        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        for (int slot = 0; slot < RESOURCE_NAMES.length; ++slot) {
            for (int state = 0; state < 2; ++state) {
                File source = GameSceneAtlasResolver.resolveResource(
                        assetRoot, RESOURCE_NAMES[slot][state]);
                if (source == null || !source.isFile()) {
                    failClosed();
                    return false;
                }

                Bitmap decoded = BitmapFactory.decodeFile(source.getAbsolutePath(), options);
                if (decoded == null) {
                    failClosed();
                    return false;
                }
                Bitmap bitmap = decoded;
                try {
                    if (decoded.getConfig() != Bitmap.Config.ARGB_8888) {
                        bitmap = decoded.copy(Bitmap.Config.ARGB_8888, false);
                        if (bitmap == null) {
                            failClosed();
                            return false;
                        }
                    }
                    int width = bitmap.getWidth();
                    int height = bitmap.getHeight();
                    long pixelCount = (long) width * (long) height;
                    if (width <= 0 || height <= 0 || width > 4096 || height > 4096 ||
                            pixelCount <= 0L || pixelCount > MAX_PIXELS ||
                            pixelCount > Integer.MAX_VALUE) {
                        failClosed();
                        return false;
                    }

                    int[] pixels = new int[(int) pixelCount];
                    bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
                    if (!nativeUploadSprite(slot, state != 0, width, height, pixels)) {
                        failClosed();
                        return false;
                    }
                    if (state == 0 && !nativeSetBounds(slot, width, height)) {
                        failClosed();
                        return false;
                    }
                } finally {
                    if (bitmap != decoded) bitmap.recycle();
                    decoded.recycle();
                }
            }
        }
        for (int slot = 0; slot < QUICK_USE_RESOURCE_NAMES.length; ++slot) {
            loadQuickUseSprite(assetRoot, options, slot);
        }
        return true;
    }

    private static void loadQuickUseSprite(
            File assetRoot,
            BitmapFactory.Options options,
            int slot) {
        File source = GameSceneAtlasResolver.resolveResource(
                assetRoot, QUICK_USE_RESOURCE_NAMES[slot]);
        if (source == null || !source.isFile()) return;
        Bitmap decoded = BitmapFactory.decodeFile(source.getAbsolutePath(), options);
        if (decoded == null) return;
        Bitmap bitmap = decoded;
        try {
            if (decoded.getConfig() != Bitmap.Config.ARGB_8888) {
                bitmap = decoded.copy(Bitmap.Config.ARGB_8888, false);
                if (bitmap == null) return;
            }
            int width = bitmap.getWidth();
            int height = bitmap.getHeight();
            long pixelCount = (long) width * (long) height;
            if (width <= 0 || height <= 0 || width > 4096 || height > 4096 ||
                    pixelCount <= 0L || pixelCount > MAX_PIXELS ||
                    pixelCount > Integer.MAX_VALUE) return;
            int[] pixels = new int[(int) pixelCount];
            bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
            if (nativeUploadQuickUseSprite(slot, width, height, pixels)) {
                nativeSetQuickUseBounds(slot, width, height);
            }
        } finally {
            if (bitmap != decoded) bitmap.recycle();
            decoded.recycle();
        }
    }

    private static void failClosed() {
        nativeClearBounds();
        nativeClearQuickUseBounds();
        nativeClearSprites();
    }

    private static native boolean nativeSetBounds(int slot, int width, int height);
    private static native void nativeClearBounds();
    private static native boolean nativeSetQuickUseBounds(int slot, int width, int height);
    private static native void nativeClearQuickUseBounds();
    private static native boolean nativeUploadSprite(
            int slot, boolean selected, int width, int height, int[] pixels);
    private static native void nativeClearSprites();
    private static native boolean nativeUploadQuickUseSprite(
            int slot, int width, int height, int[] pixels);
}
