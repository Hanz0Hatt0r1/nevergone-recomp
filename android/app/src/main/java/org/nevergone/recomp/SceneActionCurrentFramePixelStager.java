package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import java.io.File;

/** Decodes and atomically retains one evidence-backed current SceneActions frame. */
final class SceneActionCurrentFramePixelStager {
    private static final long MAX_ATLAS_PIXELS = 16L * 1024L * 1024L;

    private SceneActionCurrentFramePixelStager() {}

    private static native boolean nativeUploadFrame(
            int slot,
            int variant,
            int mode,
            int frameIndex,
            int width,
            int height,
            int[] pixels);
    private static native boolean nativeReadyFrame(int slot, int variant, int frameIndex);

    static boolean ready(int slot, int variant, int frameIndex) {
        if ((slot != SceneActionPairRuntimeLoader.SLOT_BORN &&
                slot != SceneActionPairRuntimeLoader.SLOT_IDLE) ||
                variant < 1 || variant > 4 || frameIndex < 0) {
            return false;
        }
        return nativeReadyFrame(slot, variant, frameIndex);
    }

    static boolean stage(File assetRoot, int slot, int variant, int frameIndex) {
        if (assetRoot == null ||
                (slot != SceneActionPairRuntimeLoader.SLOT_BORN &&
                        slot != SceneActionPairRuntimeLoader.SLOT_IDLE) ||
                variant < 1 || variant > 4 || frameIndex < 0) {
            return false;
        }
        if (nativeReadyFrame(slot, variant, frameIndex)) return true;

        SceneActionPairRuntimeLoader.CurrentFrame request =
                SceneActionPairRuntimeLoader.currentFrame(slot, frameIndex);
        if (request == null || request.frameIndex != frameIndex) return false;
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
                return nativeUploadFrame(
                        slot,
                        variant,
                        resolved.mode,
                        frameIndex,
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
