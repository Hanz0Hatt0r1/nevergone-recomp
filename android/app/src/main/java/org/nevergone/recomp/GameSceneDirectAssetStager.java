package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

final class GameSceneDirectAssetStager {
    private static final long MAX_PIXELS_PER_ASSET = 16L * 1024L * 1024L;
    private static final long MAX_CACHED_ATLAS_PIXELS = 32L * 1024L * 1024L;
    private static final int MAX_ATLAS_PRELOAD_NAMES = 256;
    private static final int MAX_FRAME_NAME_LENGTH = 512;
    private static final String HIDDEN_FALLBACK_ASSET = "empty.png";

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
        PlayerActionRuntimeLoader.invalidate();
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

    private static boolean safeFrameName(String value) {
        return value != null && !value.isEmpty() && value.length() <= MAX_FRAME_NAME_LENGTH &&
                value.indexOf('\0') < 0 && value.indexOf('/') < 0 && value.indexOf('\\') < 0 &&
                !".".equals(value) && !"..".equals(value);
    }

    static boolean stageIfNeeded(File assetRoot) {
        PlayerActionRuntimeLoader.stageIfNeeded(assetRoot);
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
        Map<String, Bitmap> atlasBitmaps = new LinkedHashMap<>(16, 0.75f, true);
        Map<String, GameSceneAtlasFrameResolver.Prepared> playerFrameResolvers = new HashMap<>();
        try {
            File canonicalRoot = assetRoot.getCanonicalFile();
            String rootPrefix = canonicalRoot.getPath() + File.separator;
            BitmapFactory.Options options = new BitmapFactory.Options();
            options.inPreferredConfig = Bitmap.Config.ARGB_8888;

            int[] requestKinds = new int[count];
            int[] spriteCommandIndices = new int[count];
            String[] requestValues = new String[count];
            boolean[] playerRequests = new boolean[count];
            String[] playerAtlasPlists = new String[count];
            boolean needsSceneAtlases = false;
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
                playerRequests[requestIndex] =
                        GameSceneDirectAssetRequests.isPlayerBodyAt(requestIndex);
                if (playerRequests[requestIndex] &&
                        kind != GameSceneDirectAssetRequests.KIND_SPRITE_FRAME_BY_NAME) {
                    return reject("invalid-player-request", revision, count,
                            requestIndex, kind, stagedCount);
                }
                if (kind == GameSceneDirectAssetRequests.KIND_SPRITE_FRAME_BY_NAME) {
                    if (playerRequests[requestIndex]) {
                        String playerAtlasPlist =
                                GameSceneDirectAssetRequests.playerAtlasPlistAt(requestIndex);
                        if (playerAtlasPlist == null || playerAtlasPlist.isEmpty()) {
                            return reject("player-atlas-name-unavailable", revision, count,
                                    requestIndex, kind, stagedCount);
                        }
                        playerAtlasPlists[requestIndex] = playerAtlasPlist;
                    } else {
                        needsSceneAtlases = true;
                    }
                }
            }

            List<GameSceneAtlasResolver.Atlas> sceneAtlases = null;
            GameSceneAtlasFrameResolver.Prepared sceneFrameResolver = null;
            if (needsSceneAtlases) {
                int atlasCount = GameSceneDirectAssetRequests.atlasCount();
                if (atlasCount <= 0 || atlasCount > MAX_ATLAS_PRELOAD_NAMES) {
                    return reject(
                            "atlas-preload-list-unavailable",
                            revision,
                            count,
                            -1,
                            GameSceneDirectAssetRequests.KIND_SPRITE_FRAME_BY_NAME,
                            stagedCount);
                }
                ArrayList<String> atlasNames = new ArrayList<>(atlasCount);
                for (int atlasIndex = 0; atlasIndex < atlasCount; ++atlasIndex) {
                    String atlasName = GameSceneDirectAssetRequests.atlasNameAt(atlasIndex);
                    if (atlasName == null || atlasName.isEmpty()) {
                        return reject(
                                "invalid-atlas-preload-name",
                                revision,
                                count,
                                -1,
                                GameSceneDirectAssetRequests.KIND_SPRITE_FRAME_BY_NAME,
                                stagedCount);
                    }
                    atlasNames.add(atlasName);
                }
                sceneAtlases = GameSceneAtlasResolver.resolve(canonicalRoot, atlasNames);
                if (sceneAtlases == null || sceneAtlases.isEmpty()) {
                    return reject(
                            "atlas-resolution-failed",
                            revision,
                            count,
                            -1,
                            GameSceneDirectAssetRequests.KIND_SPRITE_FRAME_BY_NAME,
                            stagedCount);
                }
                sceneFrameResolver =
                        GameSceneAtlasFrameResolver.prepare(canonicalRoot, sceneAtlases);
                if (sceneFrameResolver == null) {
                    return reject(
                            "frame-cache-prepare-failed",
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
                    File source = GameSceneAtlasResolver.resolveResource(canonicalRoot, value);
                    if (source == null) {
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

                GameSceneAtlasFrameResolver.Prepared frameResolver;
                if (playerRequests[requestIndex]) {
                    String playerAtlasPlist = playerAtlasPlists[requestIndex];
                    frameResolver = playerFrameResolvers.get(playerAtlasPlist);
                    if (frameResolver == null) {
                        List<GameSceneAtlasResolver.Atlas> playerAtlases =
                                PlayerActionAtlasCacheResolver.resolve(
                                        canonicalRoot, playerAtlasPlist);
                        if (playerAtlases == null || playerAtlases.isEmpty()) {
                            return reject(
                                    "player-atlas-resolution-failed",
                                    revision,
                                    count,
                                    requestIndex,
                                    kind,
                                    stagedCount);
                        }
                        frameResolver =
                                GameSceneAtlasFrameResolver.prepare(canonicalRoot, playerAtlases);
                        if (frameResolver == null) {
                            return reject(
                                    "frame-cache-prepare-failed",
                                    revision,
                                    count,
                                    requestIndex,
                                    kind,
                                    stagedCount);
                        }
                        playerFrameResolvers.put(playerAtlasPlist, frameResolver);
                    }
                } else {
                    frameResolver = sceneFrameResolver;
                }
                if (frameResolver == null) {
                    return reject(
                            "atlas-resolution-failed",
                            revision,
                            count,
                            requestIndex,
                            kind,
                            stagedCount);
                }
                GameSceneAtlasFrameResolver.Frame frame = frameResolver.resolve(value);
                if (frame == null) {
                    if (playerRequests[requestIndex]) {
                        // The shipped action data can name a frame absent from
                        // its selected preload family. Keep the first scene
                        // visible without inventing a player body from another
                        // selector's atlas; P8 remains explicitly unresolved.
                        if (!GameSceneDirectAssetStore.uploadHidden(
                                revision, requestIndex, spriteCommandIndex)) {
                            return reject("store-hidden-upload-failed", revision, count,
                                    requestIndex, kind, stagedCount);
                        }
                        GameSceneAssetStagingDiagnostics.missingPlayerFrame();
                        ++stagedCount;
                        GameSceneAssetStagingDiagnostics.progress(stagedCount);
                        continue;
                    }
                    if (!playerRequests[requestIndex] && safeFrameName(value)) {
                        File fallback = new File(canonicalRoot, HIDDEN_FALLBACK_ASSET)
                                .getCanonicalFile();
                        if (!fallback.getPath().startsWith(rootPrefix) || !fallback.isFile()) {
                            return reject(
                                    "hidden-fallback-file-unavailable",
                                    revision,
                                    count,
                                    requestIndex,
                                    kind,
                                    stagedCount);
                        }
                        if (!GameSceneDirectAssetStore.uploadHidden(
                                revision,
                                requestIndex,
                                spriteCommandIndex)) {
                            return reject(
                                    "store-hidden-upload-failed",
                                    revision,
                                    count,
                                    requestIndex,
                                    kind,
                                    stagedCount);
                        }
                        ++stagedCount;
                        GameSceneAssetStagingDiagnostics.progress(stagedCount);
                        continue;
                    }
                    return reject(
                            "frame-lookup-or-metadata-failed",
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
                            atlasPixels > MAX_CACHED_ATLAS_PIXELS) {
                        atlas.recycle();
                        return reject(
                                "atlas-pixel-bounds-failed",
                                revision,
                                count,
                                requestIndex,
                                kind,
                                stagedCount);
                    }

                    // This cache exists only to avoid repeated BitmapFactory
                    // work while one request revision is copied into the native
                    // pixel store. Once a frame has been copied, retaining every
                    // decoded atlas is not required for transaction correctness.
                    // Evict least-recently-used Bitmaps until the next atlas fits
                    // the bounded decode budget; an evicted sheet can be decoded
                    // again if a later frame refers to it.
                    while (cachedAtlasPixels + atlasPixels > MAX_CACHED_ATLAS_PIXELS &&
                            !atlasBitmaps.isEmpty()) {
                        Iterator<Map.Entry<String, Bitmap>> iterator =
                                atlasBitmaps.entrySet().iterator();
                        Map.Entry<String, Bitmap> eldest = iterator.next();
                        Bitmap evicted = eldest.getValue();
                        iterator.remove();
                        if (evicted != null && !evicted.isRecycled()) {
                            long evictedPixels =
                                    (long) evicted.getWidth() * (long) evicted.getHeight();
                            cachedAtlasPixels = evictedPixels > 0L &&
                                    evictedPixels <= cachedAtlasPixels
                                    ? cachedAtlasPixels - evictedPixels
                                    : 0L;
                            evicted.recycle();
                        }
                    }
                    if (cachedAtlasPixels + atlasPixels > MAX_CACHED_ATLAS_PIXELS) {
                        atlas.recycle();
                        return reject(
                                "atlas-cache-budget-failed",
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
