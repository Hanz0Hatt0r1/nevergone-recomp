package org.nevergone.recomp;

import android.graphics.BitmapFactory;

import java.io.File;

final class GameSceneActionAssetLoader {
    private static final String[] RESOURCE_NAMES = {
            "ab.png",
            "db.png",
            "jb.png"
    };

    private GameSceneActionAssetLoader() {}

    static boolean reloadFromFilesDir(String filesDir) {
        if (filesDir == null || filesDir.isEmpty()) return false;
        File assetRoot = new File(filesDir, "assets");
        if (!assetRoot.isDirectory()) return false;

        int[] widths = new int[RESOURCE_NAMES.length];
        int[] heights = new int[RESOURCE_NAMES.length];
        for (int slot = 0; slot < RESOURCE_NAMES.length; ++slot) {
            File source = GameSceneAtlasResolver.resolveResource(assetRoot, RESOURCE_NAMES[slot]);
            if (source == null || !source.isFile()) return false;

            BitmapFactory.Options options = new BitmapFactory.Options();
            options.inJustDecodeBounds = true;
            BitmapFactory.decodeFile(source.getAbsolutePath(), options);
            if (options.outWidth <= 0 || options.outHeight <= 0 ||
                    options.outWidth > 4096 || options.outHeight > 4096) {
                return false;
            }
            widths[slot] = options.outWidth;
            heights[slot] = options.outHeight;
        }

        nativeClearBounds();
        for (int slot = 0; slot < RESOURCE_NAMES.length; ++slot) {
            if (!nativeSetBounds(slot, widths[slot], heights[slot])) {
                nativeClearBounds();
                return false;
            }
        }
        return true;
    }

    private static native boolean nativeSetBounds(int slot, int width, int height);
    private static native void nativeClearBounds();
}
