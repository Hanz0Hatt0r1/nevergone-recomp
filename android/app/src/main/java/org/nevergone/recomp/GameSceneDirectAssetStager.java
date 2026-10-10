package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import java.io.File;
import java.io.IOException;

final class GameSceneDirectAssetStager {
    private static final long MAX_PIXELS_PER_ASSET = 16L * 1024L * 1024L;

    private GameSceneDirectAssetStager() {}

    static boolean stageIfNeeded(File assetRoot) {
        long revision = GameSceneDirectAssetRequests.refresh();
        if (revision == 0L) {
            GameSceneDirectAssetStore.clear();
            return true;
        }
        if (GameSceneDirectAssetStore.activeRevision() == revision) return true;
        if (assetRoot == null || !assetRoot.isDirectory()) return false;

        int count = GameSceneDirectAssetRequests.count();
        if (count < 0 || !GameSceneDirectAssetStore.begin(revision, count)) return false;

        boolean completed = false;
        try {
            File canonicalRoot = assetRoot.getCanonicalFile();
            String rootPrefix = canonicalRoot.getPath() + File.separator;
            BitmapFactory.Options options = new BitmapFactory.Options();
            options.inPreferredConfig = Bitmap.Config.ARGB_8888;

            for (int requestIndex = 0; requestIndex < count; requestIndex++) {
                String relativePath = GameSceneDirectAssetRequests.pathAt(requestIndex);
                int spriteCommandIndex =
                        GameSceneDirectAssetRequests.spriteCommandIndexAt(requestIndex);
                if (relativePath == null || relativePath.isEmpty() || spriteCommandIndex < 0) {
                    return false;
                }

                File source = new File(canonicalRoot, relativePath).getCanonicalFile();
                if (!source.getPath().startsWith(rootPrefix) || !source.isFile()) return false;

                Bitmap decoded = BitmapFactory.decodeFile(source.getAbsolutePath(), options);
                if (decoded == null) return false;
                Bitmap bitmap = decoded;
                try {
                    if (decoded.getConfig() != Bitmap.Config.ARGB_8888) {
                        Bitmap converted = decoded.copy(Bitmap.Config.ARGB_8888, false);
                        if (converted == null) return false;
                        bitmap = converted;
                    }

                    int width = bitmap.getWidth();
                    int height = bitmap.getHeight();
                    long pixelCount = (long) width * (long) height;
                    if (width <= 0 || height <= 0 ||
                            pixelCount <= 0L || pixelCount > MAX_PIXELS_PER_ASSET ||
                            pixelCount > Integer.MAX_VALUE) {
                        return false;
                    }

                    int[] pixels = new int[(int) pixelCount];
                    bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
                    if (!GameSceneDirectAssetStore.upload(
                            revision,
                            requestIndex,
                            spriteCommandIndex,
                            width,
                            height,
                            pixels)) {
                        return false;
                    }
                } finally {
                    if (bitmap != decoded) bitmap.recycle();
                    decoded.recycle();
                }
            }

            completed = GameSceneDirectAssetStore.finish(revision);
            return completed;
        } catch (IOException | RuntimeException error) {
            return false;
        } finally {
            if (!completed) GameSceneDirectAssetStore.cancel(revision);
        }
    }
}
