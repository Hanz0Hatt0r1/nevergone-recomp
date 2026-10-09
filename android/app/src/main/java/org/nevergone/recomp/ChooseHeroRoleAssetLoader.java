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
    private static final int FOCUS_HIGHLIGHT_INDEX = 8;
    private static final String FOCUS_HIGHLIGHT_PATH =
            "gamescene_ui/LevelUI/gate_chapter_ui/act_hilight.png";

    private static native void nativeClear();
    private static native boolean nativeUpload(int index, int width, int height, int[] pixels);
    private static native boolean nativeReady();

    private ChooseHeroRoleAssetLoader() {}

    static boolean reloadFromFilesDir(String filesDir) {
        nativeClear();
        if (filesDir == null || filesDir.isEmpty()) return false;

        File assetsRoot = new File(filesDir, "assets");
        File directory = new File(assetsRoot, RELATIVE_DIR);
        for (int index = 0; index < FILES.length; index++) {
            if (!decodeAndUpload(index, new File(directory, FILES[index]))) {
                nativeClear();
                return false;
            }
        }

        // Optional OBB/update-era focus texture. The baseline role tiles remain
        // usable when this file is absent.
        File focus = new File(assetsRoot, FOCUS_HIGHLIGHT_PATH);
        if (focus.isFile()) {
            decodeAndUpload(FOCUS_HIGHLIGHT_INDEX, focus);
        }
        return nativeReady();
    }

    private static boolean decodeAndUpload(int index, File source) {
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
        return nativeUpload(index, width, height, pixels);
    }
}
