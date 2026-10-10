package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import java.io.File;
import java.io.IOException;
import java.util.List;

final class GameSceneFrameAssetStager {
    private static final long MAX_ATLAS_PIXELS = 32L * 1024L * 1024L;
    private static final String GAME_DATA_FILE = "pvp_scene.glData";

    // Calls are serialized on the GLSurfaceView render thread. Cache one failed
    // revision so malformed/missing imported atlas data cannot be reparsed and
    // decoded on every frame.
    private static long failedRevision;

    private static final class ResolvedFrame {
        final TexturePackerPlist.Frame frame;
        final File textureFile;

        ResolvedFrame(TexturePackerPlist.Frame frame, File textureFile) {
            this.frame = frame;
            this.textureFile = textureFile;
        }
    }

    private GameSceneFrameAssetStager() {}

    static void resetFailure() {
        failedRevision = 0L;
    }

    static void invalidate() {
        failedRevision = 0L;
        GameSceneFrameAssetStore.clear();
    }

    static boolean stageIfNeeded(File assetRoot) {
        long revision = GameSceneFrameAssetRequests.refresh();
        long activeRevision = GameSceneFrameAssetStore.activeRevision();
        if (revision == 0L) {
            failedRevision = 0L;
            if (activeRevision != 0L) GameSceneFrameAssetStore.clear();
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

        int count = GameSceneFrameAssetRequests.count();
        if (count < 0 || !GameSceneFrameAssetStore.begin(revision, count)) {
            failedRevision = revision;
            return false;
        }

        boolean completed = false;
        try {
            if (count == 0) {
                completed = GameSceneFrameAssetStore.finish(revision);
                if (completed) failedRevision = 0L;
                return completed;
            }

            File canonicalRoot = assetRoot.getCanonicalFile();
            String rootPrefix = canonicalRoot.getPath() + File.separator;
            GameSceneAtlasListDiscovery.Result discovery =
                    GameSceneAtlasListDiscovery.discover(canonicalRoot, GAME_DATA_FILE);
            if (discovery == null) return false;
            List<GameSceneAtlasResolver.Atlas> atlases =
                    GameSceneAtlasResolver.resolve(canonicalRoot, discovery);
            if (atlases == null || atlases.isEmpty()) return false;

            for (int requestIndex = 0; requestIndex < count; requestIndex++) {
                String frameName = GameSceneFrameAssetRequests.frameNameAt(requestIndex);
                int spriteCommandIndex =
                        GameSceneFrameAssetRequests.spriteCommandIndexAt(requestIndex);
                if (frameName == null || frameName.isEmpty() || spriteCommandIndex < 0) {
                    return false;
                }

                ResolvedFrame resolved = resolveFirstDirectFrame(
                        canonicalRoot, rootPrefix, atlases, frameName);
                if (resolved == null) return false;

                Bitmap atlas = decodeBoundedArgb(resolved.textureFile);
                if (atlas == null) return false;
                try {
                    TexturePackerAtlasExtractor.ExtractedFrame extracted =
                            TexturePackerAtlasExtractor.extract(resolved.frame, atlas);
                    if (extracted == null || !GameSceneFrameAssetStore.upload(
                            revision,
                            requestIndex,
                            spriteCommandIndex,
                            extracted.width,
                            extracted.height,
                            extracted.sourceWidth,
                            extracted.sourceHeight,
                            extracted.left,
                            extracted.top,
                            extracted.pixels)) {
                        return false;
                    }
                } finally {
                    atlas.recycle();
                }
            }

            completed = GameSceneFrameAssetStore.finish(revision);
            if (completed) failedRevision = 0L;
            return completed;
        } catch (IOException | RuntimeException error) {
            return false;
        } catch (Exception error) {
            return false;
        } finally {
            if (!completed) {
                GameSceneFrameAssetStore.cancel(revision);
                failedRevision = revision;
            }
        }
    }

    private static ResolvedFrame resolveFirstDirectFrame(
            File assetRoot,
            String rootPrefix,
            List<GameSceneAtlasResolver.Atlas> atlases,
            String frameName) throws Exception {
        // cocos2d-x 2.1.2 skips a frame when m_pSpriteFrames already contains
        // its name. Because loadingTex() preloads gsresfile04 in array order,
        // the first atlas defining a direct frame name wins.
        for (GameSceneAtlasResolver.Atlas atlas : atlases) {
            File plist = new File(assetRoot, atlas.plistRelativePath).getCanonicalFile();
            if (!plist.getPath().startsWith(rootPrefix) || !plist.isFile()) return null;
            TexturePackerPlist.Frame frame = TexturePackerPlist.readFrame(plist, frameName);
            if (frame == null) continue;

            File texture = new File(assetRoot, atlas.textureRelativePath).getCanonicalFile();
            if (!texture.getPath().startsWith(rootPrefix) || !texture.isFile()) return null;
            return new ResolvedFrame(frame, texture);
        }
        // Alias lookup remains a separate bounded milestone. Failing closed is
        // preferable to mapping an alias to the wrong atlas/frame.
        return null;
    }

    private static Bitmap decodeBoundedArgb(File source) {
        BitmapFactory.Options bounds = new BitmapFactory.Options();
        bounds.inJustDecodeBounds = true;
        BitmapFactory.decodeFile(source.getAbsolutePath(), bounds);
        if (bounds.outWidth <= 0 || bounds.outHeight <= 0) return null;
        long pixelCount = (long) bounds.outWidth * (long) bounds.outHeight;
        if (pixelCount <= 0L || pixelCount > MAX_ATLAS_PIXELS) return null;

        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        Bitmap decoded = BitmapFactory.decodeFile(source.getAbsolutePath(), options);
        if (decoded == null) return null;
        if (decoded.getConfig() == Bitmap.Config.ARGB_8888) return decoded;

        Bitmap converted = decoded.copy(Bitmap.Config.ARGB_8888, false);
        decoded.recycle();
        return converted;
    }
}
