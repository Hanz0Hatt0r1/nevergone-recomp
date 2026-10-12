package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import java.io.File;

final class TutorialLoadingAssetLoader {
    private static final String LOADING_DIR = "gamescene_ui/loading_UI";
    private static final String[] FRAME_NAMES = {
            "logo_01.png",
            "logo_02.png",
            "logo_03.png",
    };

    private static native void nativeClearFrames();
    private static native boolean nativeUploadFrame(
            int frameIndex, int width, int height, int[] argbPixels);

    private TutorialLoadingAssetLoader() {}

    static boolean reloadFromFilesDir(String filesDir) {
        nativeClearFrames();
        if (filesDir == null || filesDir.isEmpty()) return false;

        File assetRoot = new File(filesDir, "assets");
        File directory = new File(assetRoot, LOADING_DIR);
        int loaded = 0;
        for (int index = 0; index < FRAME_NAMES.length; index++) {
            File frame = new File(directory, FRAME_NAMES[index]);
            if (!frame.isFile()) {
                frame = findFile(assetRoot, FRAME_NAMES[index], 0);
            }
            if (frame == null || !frame.isFile()) continue;

            BitmapFactory.Options options = new BitmapFactory.Options();
            options.inPreferredConfig = Bitmap.Config.ARGB_8888;
            Bitmap decoded = BitmapFactory.decodeFile(frame.getAbsolutePath(), options);
            if (decoded == null) continue;

            Bitmap bitmap = decoded;
            if (decoded.getConfig() != Bitmap.Config.ARGB_8888) {
                Bitmap converted = decoded.copy(Bitmap.Config.ARGB_8888, false);
                decoded.recycle();
                if (converted == null) continue;
                bitmap = converted;
            }

            int width = bitmap.getWidth();
            int height = bitmap.getHeight();
            if (width <= 0 || height <= 0 ||
                    ((long) width * (long) height) > 16_777_216L) {
                bitmap.recycle();
                continue;
            }

            int[] pixels = new int[width * height];
            bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
            bitmap.recycle();
            if (nativeUploadFrame(index, width, height, pixels)) loaded++;
        }
        return loaded > 0;
    }

    private static File findFile(File directory, String targetName, int depth) {
        if (directory == null || !directory.isDirectory() || depth > 16) return null;
        File[] children = directory.listFiles();
        if (children == null) return null;
        for (File child : children) {
            if (child.isFile() && targetName.equals(child.getName())) return child;
        }
        for (File child : children) {
            if (!child.isDirectory()) continue;
            File found = findFile(child, targetName, depth + 1);
            if (found != null) return found;
        }
        return null;
    }
}
