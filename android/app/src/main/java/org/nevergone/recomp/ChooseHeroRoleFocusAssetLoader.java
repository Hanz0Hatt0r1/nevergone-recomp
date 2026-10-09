package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import java.io.File;

final class ChooseHeroRoleFocusAssetLoader {
    private static final String RELATIVE_PATH =
            "assets/gamescene_ui/LevelUI/gate_chapter_ui/act_hilight.png";

    private static native void nativeClear();
    private static native boolean nativeUpload(int width, int height, int[] pixels);
    private static native boolean nativeReady();

    private ChooseHeroRoleFocusAssetLoader() {}

    static boolean reloadFromFilesDir(String filesDir) {
        nativeClear();
        if (filesDir == null || filesDir.isEmpty()) return false;
        File source = new File(filesDir, RELATIVE_PATH);
        if (!source.isFile()) return false;

        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        Bitmap decoded = BitmapFactory.decodeFile(source.getAbsolutePath(), options);
        if (decoded == null) return false;
        Bitmap bitmap = decoded;
        if (decoded.getConfig() != Bitmap.Config.ARGB_8888) {
            Bitmap converted = decoded.copy(Bitmap.Config.ARGB_8888, false);
            decoded.recycle();
            if (converted == null) return false;
            bitmap = converted;
        }

        int width = bitmap.getWidth();
        int height = bitmap.getHeight();
        if (width <= 0 || height <= 0 || ((long) width * (long) height) > 16_777_216L) {
            bitmap.recycle();
            return false;
        }
        int[] pixels = new int[width * height];
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
        bitmap.recycle();
        return nativeUpload(width, height, pixels) && nativeReady();
    }
}
