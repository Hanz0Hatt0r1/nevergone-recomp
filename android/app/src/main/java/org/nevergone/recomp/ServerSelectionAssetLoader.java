package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import java.io.File;

final class ServerSelectionAssetLoader {
    private static final String[] FILES = {
            "gamescene_ui/ServerList/border1.png",
            "gamescene_ui/ServerList/border2.png"
    };

    private static native void nativeClear();
    private static native boolean nativeUpload(
            int index, int width, int height, int[] pixels);

    private ServerSelectionAssetLoader() {}

    static boolean reloadFromFilesDir(String filesDir) {
        nativeClear();
        if (filesDir == null || filesDir.isEmpty()) return false;
        File assetRoot = new File(filesDir, "assets");
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        boolean uploadedAny = false;
        for (int index = 0; index < FILES.length; index++) {
            Bitmap decoded = BitmapFactory.decodeFile(
                    new File(assetRoot, FILES[index]).getAbsolutePath(), options);
            if (decoded == null) continue;
            try {
                if (!upload(index, decoded)) {
                    nativeClear();
                    return false;
                }
                uploadedAny = true;
            } finally {
                decoded.recycle();
            }
        }
        return uploadedAny;
    }

    private static boolean upload(int index, Bitmap source) {
        Bitmap bitmap = source;
        if (source.getConfig() != Bitmap.Config.ARGB_8888) {
            bitmap = source.copy(Bitmap.Config.ARGB_8888, false);
            if (bitmap == null) return false;
        }
        try {
            int width = bitmap.getWidth();
            int height = bitmap.getHeight();
            if (width <= 0 || height <= 0 || ((long) width * (long) height) > 16_777_216L) {
                return false;
            }
            int[] pixels = new int[width * height];
            bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
            return nativeUpload(index, width, height, pixels);
        } finally {
            if (bitmap != source) bitmap.recycle();
        }
    }
}
