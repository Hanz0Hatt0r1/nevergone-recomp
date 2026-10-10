package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import java.io.File;
import java.io.IOException;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

final class GameSceneDirectAssetStager {
    private static final long MAX_PIXELS_PER_ASSET = 16L * 1024L * 1024L;
    private static final long MAX_CACHED_ATLAS_PIXELS = 32L * 1024L * 1024L;
    private static final String PVP_GAME_DATA_FILE = "pvp_scene.glData";

    // All calls are made on the GLSurfaceView render thread. Remember one failed
    // queue revision so a missing/corrupt imported file cannot trigger a full
    // BitmapFactory decode attempt on every rendered frame.
    private static long failedRevision;

    private GameSceneDirectAssetStager() {}

    static void resetFailure() {
        failedRevision = 0L;
    }

    // Explicit imported-asset reloads may replace a file without changing the
    // native scene/render-queue revision. Drop the active pixel snapshot so the
    // same queue revision is decoded again from the refreshed user-owned files.
    static void invalidate() {
        failedRevision = 0L;
        GameSceneDirectAssetStore.clear();
    }

    static boolean stageIfNeeded(File assetRoot) {
        long revision = GameSceneDirectAssetRequests.refresh();
        long activeRevision = GameSceneDirectAssetStore.activeRevision();
        if (revision == 0L) {
            failedRevision = 0L;
            if (activeRevision != 0L) GameSceneDirectAssetStore.clear();
            return true;
        }
        if (activeRevision == revision) {
            failedRevision = 0L;
            return true;
        }
        if (failedRevision == revision) return false;
        if (assetRoot == null || !assetRoot.isDirectory()) {
            failedRevision = revision;
            return false;
        }

        int count = GameSceneDirectAssetRequests.count();
        if (count < 0) {
            failedRevision = revision;
            return false;
        }

        boolean completed = false;
        Map<String, Bitmap> atlasBitmaps = new HashMap<>();
        try {
            File canonicalRoot = assetRoot.getCanonicalFile();
            String rootPrefix = canonicalRoot.getPath() + File.separator;
            BitmapFactory.Options options = new BitmapFactory.Options();
            options.inPreferredConfig = Bitmap.Config.ARGB_8888;

            int[] requestKinds = new int[count];
            int[] spriteCommandIndices = new int[count];
            String[] requestValues = new String[count];
            boolean needsAtlases = false;
            for (int requestIndex = 0; requestIndex < count; ++requestIndex) {
                int kind = GameSceneDirectAssetRequests.kindAt(requestIndex);
                String value = GameSceneDirectAssetRequests.valueAt(requestIndex);
                int spriteCommandIndex =
                        GameSceneDirectAssetRequests.spriteCommandIndexAt(requestIndex);
                if ((kind != GameSceneDirectAssetRequests.KIND_DIRECT_FILE &&
                        kind != GameSceneDirectAssetRequests.KIND_SPRITE_FRAME_BY_NAME) ||
                        value == null || value.isEmpty() || spriteCommandIndex < 0) {
                    return false;
                }
                requestKinds[requestIndex] = kind;
                requestValues[requestIndex] = value;
                spriteCommandIndices[requestIndex] = spriteCommandIndex;
                needsAtlases |= kind == GameSceneDirectAssetRequests.KIND_SPRITE_FRAME_BY_NAME;
            }

            List<GameSceneAtlasResolver.Atlas> atlases = null;
            if (needsAtlases) {
                GameSceneAtlasListDiscovery.Result discovery =
                        GameSceneAtlasListDiscovery.discover(canonicalRoot, PVP_GAME_DATA_FILE);
                if (discovery == null) return false;
                atlases = GameSceneAtlasResolver.resolve(canonicalRoot, discovery);
                if (atlases == null || atlases.isEmpty()) return false;
            }

            if (!GameSceneDirectAssetStore.begin(revision, count)) return false;

            long cachedAtlasPixels = 0L;
            for (int requestIndex = 0; requestIndex < count; requestIndex++) {
                final int kind = requestKinds[requestIndex];
                final String value = requestValues[requestIndex];
                final int spriteCommandIndex = spriteCommandIndices[requestIndex];

                if (kind == GameSceneDirectAssetRequests.KIND_DIRECT_FILE) {
                    File source = new File(canonicalRoot, value).getCanonicalFile();
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
                    continue;
                }

                if (atlases == null) return false;
                GameSceneAtlasFrameResolver.Frame frame =
                        GameSceneAtlasFrameResolver.resolve(canonicalRoot, atlases, value);
                if (frame == null) return false;

                File atlasFile = new File(canonicalRoot, frame.textureRelativePath).getCanonicalFile();
                if (!atlasFile.getPath().startsWith(rootPrefix) || !atlasFile.isFile()) return false;
                String atlasPath = atlasFile.getPath();
                Bitmap atlas = atlasBitmaps.get(atlasPath);
                if (atlas == null) {
                    atlas = BitmapFactory.decodeFile(atlasPath, options);
                    if (atlas == null) return false;
                    long atlasPixels = (long) atlas.getWidth() * (long) atlas.getHeight();
                    if (atlas.getWidth() <= 0 || atlas.getHeight() <= 0 ||
                            atlasPixels <= 0L || atlasPixels > MAX_PIXELS_PER_ASSET ||
                            cachedAtlasPixels + atlasPixels > MAX_CACHED_ATLAS_PIXELS) {
                        atlas.recycle();
                        return false;
                    }
                    cachedAtlasPixels += atlasPixels;
                    atlasBitmaps.put(atlasPath, atlas);
                }

                int packedWidth = GameSceneAtlasFramePixels.packedWidth(frame);
                int packedHeight = GameSceneAtlasFramePixels.packedHeight(frame);
                long packedCount = (long) packedWidth * (long) packedHeight;
                if (packedWidth <= 0 || packedHeight <= 0 ||
                        packedCount <= 0L || packedCount > Integer.MAX_VALUE ||
                        frame.textureX < 0 || frame.textureY < 0 ||
                        (long) frame.textureX + packedWidth > atlas.getWidth() ||
                        (long) frame.textureY + packedHeight > atlas.getHeight()) {
                    return false;
                }

                int[] packedPixels = new int[(int) packedCount];
                atlas.getPixels(
                        packedPixels,
                        0,
                        packedWidth,
                        frame.textureX,
                        frame.textureY,
                        packedWidth,
                        packedHeight);
                GameSceneAtlasFramePixels.Asset reconstructed =
                        GameSceneAtlasFramePixels.reconstruct(
                                frame, packedWidth, packedHeight, packedPixels);
                if (reconstructed == null || !GameSceneDirectAssetStore.upload(
                        revision,
                        requestIndex,
                        spriteCommandIndex,
                        reconstructed.width,
                        reconstructed.height,
                        reconstructed.pixels)) {
                    return false;
                }
            }

            completed = GameSceneDirectAssetStore.finish(revision);
            if (completed) failedRevision = 0L;
            return completed;
        } catch (IOException | RuntimeException error) {
            return false;
        } finally {
            for (Bitmap bitmap : atlasBitmaps.values()) {
                if (bitmap != null && !bitmap.isRecycled()) bitmap.recycle();
            }
            if (!completed) {
                GameSceneDirectAssetStore.cancel(revision);
                failedRevision = revision;
            }
        }
    }
}
