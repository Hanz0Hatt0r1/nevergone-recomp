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
        GameSceneAssetStagingDiagnostics.reset();
    }

    // Explicit imported-asset reloads may replace a file without changing the
    // native scene/render-queue revision. Drop the active pixel snapshot so the
    // same queue revision is decoded again from the refreshed user-owned files.
    static void invalidate() {
        failedRevision = 0L;
        GameSceneAssetStagingDiagnostics.reset();
        GameSceneDirectAssetStore.clear();
    }

    static String statusReport() {
        return GameSceneAssetStagingDiagnostics.statusReport();
    }

    private static boolean reject(
            String reason,
            long revision,
            int requestCount,
            int requestIndex,
            int requestKind,
            int stagedCount) {
        failedRevision = revision;
        GameSceneAssetStagingDiagnostics.failed(
                reason,
                revision,
                requestCount,
                requestIndex,
                requestKind,
                stagedCount);
        return false;
    }

    static boolean stageIfNeeded(File assetRoot) {
        long revision = GameSceneDirectAssetRequests.refresh();
        long activeRevision = GameSceneDirectAssetStore.activeRevision();
        if (revision == 0L) {
            failedRevision = 0L;
            if (activeRevision != 0L) GameSceneDirectAssetStore.clear();
            GameSceneAssetStagingDiagnostics.noLiveQueue();
            return true;
        }

        int count = GameSceneDirectAssetRequests.count();
        if (count < 0) {
            return reject("invalid-request-count", revision, 0, -1, -1, 0);
        }
        if (activeRevision == revision) {
            failedRevision = 0L;
            GameSceneAssetStagingDiagnostics.ready(revision, count);
            return true;
        }
        if (failedRevision == revision) return false;

        GameSceneAssetStagingDiagnostics.begin(revision, count);
        if (assetRoot == null || !assetRoot.isDirectory()) {
            return reject("asset-root-unavailable", revision, count, -1, -1, 0);
        }

        boolean completed = false;
        int currentRequestIndex = -1;
        int currentRequestKind = -1;
        int stagedCount = 0;
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
                    return reject(
                            "invalid-request",
                            revision,
                            count,
                            requestIndex,
                            kind,
                            stagedCount);
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
                if (discovery == null) {
                    return reject(
                            "atlas-list-discovery-failed",
                            revision,
                            count,
                            -1,
                            GameSceneDirectAssetRequests.KIND_SPRITE_FRAME_BY_NAME,
                            stagedCount);
                }
                atlases = GameSceneAtlasResolver.resolve(canonicalRoot, discovery);
                if (atlases == null || atlases.isEmpty()) {
                    return reject(
                            "atlas-resolution-failed",
                            revision,
                            count,
                            -1,
                            GameSceneDirectAssetRequests.KIND_SPRITE_FRAME_BY_NAME,
                            stagedCount);
                }
            }

            if (!GameSceneDirectAssetStore.begin(revision, count)) {
                return reject("store-begin-failed", revision, count, -1, -1, stagedCount);
            }

            long cachedAtlasPixels = 0L;
            for (int requestIndex = 0; requestIndex < count; requestIndex++) {
                currentRequestIndex = requestIndex;
                final int kind = requestKinds[requestIndex];
                currentRequestKind = kind;
                final String value = requestValues[requestIndex];
                final int spriteCommandIndex = spriteCommandIndices[requestIndex];

                if (kind == GameSceneDirectAssetRequests.KIND_DIRECT_FILE) {
                    File source = new File(canonicalRoot, value).getCanonicalFile();
                    if (!source.getPath().startsWith(rootPrefix) || !source.isFile()) {
                        return reject(
                                "direct-file-unavailable",
                                revision,
                                count,
                                requestIndex,
                                kind,
                                stagedCount);
                    }

                    Bitmap decoded = BitmapFactory.decodeFile(source.getAbsolutePath(), options);
                    if (decoded == null) {
                        return reject(
                                "direct-decode-failed",
                                revision,
                                count,
                                requestIndex,
                                kind,
                                stagedCount);
                    }
                    Bitmap bitmap = decoded;
                    try {
                        if (decoded.getConfig() != Bitmap.Config.ARGB_8888) {
                            Bitmap converted = decoded.copy(Bitmap.Config.ARGB_8888, false);
                            if (converted == null) {
                                return reject(
                                        "direct-decode-failed",
                                        revision,
                                        count,
                                        requestIndex,
                                        kind,
                                        stagedCount);
                            }
                            bitmap = converted;
                        }

                        int width = bitmap.getWidth();
                        int height = bitmap.getHeight();
                        long pixelCount = (long) width * (long) height;
                        if (width <= 0 || height <= 0 ||
                                pixelCount <= 0L || pixelCount > MAX_PIXELS_PER_ASSET ||
                                pixelCount > Integer.MAX_VALUE) {
                            return reject(
                                    "direct-pixel-bounds-failed",
                                    revision,
                                    count,
                                    requestIndex,
                                    kind,
                                    stagedCount);
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
                            return reject(
                                    "store-upload-failed",
                                    revision,
                                    count,
                                    requestIndex,
                                    kind,
                                    stagedCount);
                        }
                    } finally {
                        if (bitmap != decoded) bitmap.recycle();
                        decoded.recycle();
                    }
                    ++stagedCount;
                    GameSceneAssetStagingDiagnostics.progress(stagedCount);
                    continue;
                }

                if (atlases == null) {
                    return reject(
                            "atlas-resolution-failed",
                            revision,
                            count,
                            requestIndex,
                            kind,
                            stagedCount);
                }
                GameSceneAtlasFrameResolver.Frame frame =
                        GameSceneAtlasFrameResolver.resolve(canonicalRoot, atlases, value);
                if (frame == null) {
                    return reject(
                            "frame-resolution-failed",
                            revision,
                            count,
                            requestIndex,
                            kind,
                            stagedCount);
                }

                File atlasFile = new File(canonicalRoot, frame.textureRelativePath).getCanonicalFile();
                if (!atlasFile.getPath().startsWith(rootPrefix) || !atlasFile.isFile()) {
                    return reject(
                            "atlas-file-unavailable",
                            revision,
                            count,
                            requestIndex,
                            kind,
                            stagedCount);
                }
                String atlasPath = atlasFile.getPath();
                Bitmap atlas = atlasBitmaps.get(atlasPath);
                if (atlas == null) {
                    atlas = BitmapFactory.decodeFile(atlasPath, options);
                    if (atlas == null) {
                        return reject(
                                "atlas-decode-failed",
                                revision,
                                count,
                                requestIndex,
                                kind,
                                stagedCount);
                    }
                    long atlasPixels = (long) atlas.getWidth() * (long) atlas.getHeight();
                    if (atlas.getWidth() <= 0 || atlas.getHeight() <= 0 ||
                            atlasPixels <= 0L || atlasPixels > MAX_PIXELS_PER_ASSET ||
                            cachedAtlasPixels + atlasPixels > MAX_CACHED_ATLAS_PIXELS) {
                        atlas.recycle();
                        return reject(
                                "atlas-pixel-bounds-failed",
                                revision,
                                count,
                                requestIndex,
                                kind,
                                stagedCount);
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
                    return reject(
                            "frame-crop-bounds-failed",
                            revision,
                            count,
                            requestIndex,
                            kind,
                            stagedCount);
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
                if (reconstructed == null) {
                    return reject(
                            "frame-reconstruction-failed",
                            revision,
                            count,
                            requestIndex,
                            kind,
                            stagedCount);
                }
                if (!GameSceneDirectAssetStore.upload(
                        revision,
                        requestIndex,
                        spriteCommandIndex,
                        reconstructed.width,
                        reconstructed.height,
                        reconstructed.pixels)) {
                    return reject(
                            "store-upload-failed",
                            revision,
                            count,
                            requestIndex,
                            kind,
                            stagedCount);
                }
                ++stagedCount;
                GameSceneAssetStagingDiagnostics.progress(stagedCount);
            }

            completed = GameSceneDirectAssetStore.finish(revision);
            if (!completed) {
                return reject(
                        "store-finish-failed",
                        revision,
                        count,
                        currentRequestIndex,
                        currentRequestKind,
                        stagedCount);
            }
            failedRevision = 0L;
            GameSceneAssetStagingDiagnostics.succeeded(revision, count);
            return true;
        } catch (IOException error) {
            return reject(
                    "io-failure",
                    revision,
                    count,
                    currentRequestIndex,
                    currentRequestKind,
                    stagedCount);
        } catch (RuntimeException error) {
            return reject(
                    "runtime-failure",
                    revision,
                    count,
                    currentRequestIndex,
                    currentRequestKind,
                    stagedCount);
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
