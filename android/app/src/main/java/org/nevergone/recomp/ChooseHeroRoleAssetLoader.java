package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import java.io.File;

final class ChooseHeroRoleAssetLoader {
    private static final String RELATIVE_DIR = "Login/ChooseHero";
    private static final String[] FILES = {
            "hero_board_a.png",
            "hero_board_b.png",
            "Hero_01_a.png",
            "Hero_01_b.png",
            "Hero_02_a.png",
            "Hero_02_b.png",
            "create_add_a.png",
            "create_add_b.png"
    };

    private static native void nativeClear();
    private static native boolean nativeUpload(int index, int width, int height, int[] pixels);
    private static native boolean nativeReady();

    private ChooseHeroRoleAssetLoader() {}

    static boolean reloadFromFilesDir(String filesDir) {
        nativeClear();
        if (filesDir == null || filesDir.isEmpty()) return false;

        File directory = new File(new File(filesDir, "assets"), RELATIVE_DIR);
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;

        for (int index = 0; index < FILES.length; index++) {
            File source = new File(directory, FILES[index]);
            Bitmap decoded = BitmapFactory.decodeFile(source.getAbsolutePath(), options);
            if (decoded == null) {
                nativeClear();
                return false;
            }

            Bitmap bitmap = decoded;
            if (decoded.getConfig() != Bitmap.Config.ARGB_8888) {
                Bitmap converted = decoded.copy(Bitmap.Config.ARGB_8888, false);
                decoded.recycle();
                if (converted == null) {
                    nativeClear();
                    return false;
                }
                bitmap = converted;
            }

            int width = bitmap.getWidth();
            int height = bitmap.getHeight();
            if (width <= 0 || height <= 0 || ((long) width * (long) height) > 16_777_216L) {
                bitmap.recycle();
                nativeClear();
                return false;
            }
            int[] pixels = new int[width * height];
            bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
            bitmap.recycle();
            if (!nativeUpload(index, width, height, pixels)) {
                nativeClear();
                return false;
            }
        }
        return nativeReady();
    }
}
