package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import java.io.File;

/** Decodes and retains one evidence-backed SceneActions initial sprite frame. */
final class SceneActionInitialFramePixelStager {
    private static final long MAX_ATLAS_PIXELS = 16L * 1024L * 1024L;

    private SceneActionInitialFramePixelStager() {}

    private static native boolean nativeUpload(
            int slot,
            int variant,
            int mode,
            int width,
            int height,
            int[] pixels);
    private static native boolean nativeReady(int slot, int variant);
    private static native void nativeReset();

    static void invalidate() {
        nativeReset();
    }

    static boolean ready(int slot, int variant) {
        if ((slot != SceneActionPairRuntimeLoader.SLOT_BORN &&
                slot != SceneActionPairRuntimeLoader.SLOT_IDLE) ||
                variant < 1 || variant > 4) {
            return false;
        }
        return nativeReady(slot, variant);
    }

    static boolean stage(File assetRoot, int slot, int variant) {
        if (assetRoot == null ||
                (slot != SceneActionPairRuntimeLoader.SLOT_BORN &&
                        slot != SceneActionPairRuntimeLoader.SLOT_IDLE) ||
                variant < 1 || variant > 4) {
            return false;
        }
        if (nativeReady(slot, variant)) return true;

        SceneActionPairRuntimeLoader.InitialFrame request =
                SceneActionPairRuntimeLoader.initialFrame(slot);
        if (request == null) return false;
        SceneActionInitialFrameResolver.Result resolved =
                SceneActionInitialFrameResolver.resolve(assetRoot, request);
        if (resolved == null || resolved.mode != request.mode) return false;

        try {
            File root = assetRoot.getCanonicalFile();
            if (!root.isDirectory()) return false;
            String rootPrefix = root.getPath() + File.separator;
            File texture = new File(root, resolved.frame.textureRelativePath).getCanonicalFile();
            if (!texture.getPath().startsWith(rootPrefix) || !texture.isFile()) return false;

            BitmapFactory.Options options = new BitmapFactory.Options();
            options.inPreferredConfig = Bitmap.Config.ARGB_8888;
            Bitmap decoded = BitmapFactory.decodeFile(texture.getAbsolutePath(), options);
            if (decoded == null) return false;
            Bitmap bitmap = decoded;
            try {
                if (decoded.getConfig() != Bitmap.Config.ARGB_8888) {
                    Bitmap converted = decoded.copy(Bitmap.Config.ARGB_8888, false);
                    if (converted == null) return false;
                    bitmap = converted;
                }

                long atlasPixels = (long) bitmap.getWidth() * (long) bitmap.getHeight();
                if (bitmap.getWidth() <= 0 || bitmap.getHeight() <= 0 ||
                        atlasPixels <= 0L || atlasPixels > MAX_ATLAS_PIXELS) {
                    return false;
                }

                int packedWidth = GameSceneAtlasFramePixels.packedWidth(resolved.frame);
                int packedHeight = GameSceneAtlasFramePixels.packedHeight(resolved.frame);
                long packedCount = (long) packedWidth * (long) packedHeight;
                if (packedWidth <= 0 || packedHeight <= 0 ||
                        packedCount <= 0L || packedCount > Integer.MAX_VALUE ||
                        resolved.frame.textureX < 0 || resolved.frame.textureY < 0 ||
                        (long) resolved.frame.textureX + packedWidth > bitmap.getWidth() ||
                        (long) resolved.frame.textureY + packedHeight > bitmap.getHeight()) {
                    return false;
                }

                int[] packed = new int[(int) packedCount];
                bitmap.getPixels(
                        packed,
                        0,
                        packedWidth,
                        resolved.frame.textureX,
                        resolved.frame.textureY,
                        packedWidth,
                        packedHeight);
                GameSceneAtlasFramePixels.Asset asset = GameSceneAtlasFramePixels.reconstruct(
                        resolved.frame,
                        packedWidth,
                        packedHeight,
                        packed);
                if (asset == null || asset.width <= 0 || asset.height <= 0 ||
                        asset.pixels == null || asset.pixels.length != asset.width * asset.height) {
                    return false;
                }
                return nativeUpload(
                        slot,
                        variant,
                        resolved.mode,
                        asset.width,
                        asset.height,
                        asset.pixels);
            } finally {
                if (bitmap != decoded) bitmap.recycle();
                decoded.recycle();
            }
        } catch (Exception error) {
            return false;
        }
    }
}
