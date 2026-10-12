package org.nevergone.recomp;

import java.io.File;
import java.util.Collections;
import java.util.List;

/** Resolves one retained SceneActions frame request through the recovered Cocos atlas path. */
final class SceneActionInitialFrameResolver {
    static final class Result {
        final int mode;
        final GameSceneAtlasFrameResolver.Frame frame;

        Result(int mode, GameSceneAtlasFrameResolver.Frame frame) {
            this.mode = mode;
            this.frame = frame;
        }
    }

    private SceneActionInitialFrameResolver() {}

    static Result resolve(
            File assetRoot,
            SceneActionPairRuntimeLoader.InitialFrame request) {
        if (request == null) return null;
        return resolve(assetRoot, request.atlasPlist, request.frameName, request.mode);
    }

    static Result resolve(
            File assetRoot,
            SceneActionPairRuntimeLoader.CurrentFrame request) {
        if (request == null) return null;
        return resolve(assetRoot, request.atlasPlist, request.frameName, request.mode);
    }

    private static Result resolve(
            File assetRoot,
            String atlasPlist,
            String frameName,
            int mode) {
        if (assetRoot == null || (mode != 0 && mode != 1) ||
                atlasPlist == null || atlasPlist.isEmpty() ||
                frameName == null || frameName.isEmpty()) {
            return null;
        }

        List<GameSceneAtlasResolver.Atlas> atlases = GameSceneAtlasResolver.resolve(
                assetRoot,
                Collections.singletonList(atlasPlist));
        if (atlases == null || atlases.size() != 1) return null;

        GameSceneAtlasFrameResolver.Prepared prepared =
                GameSceneAtlasFrameResolver.prepare(assetRoot, atlases);
        if (prepared == null) return null;
        GameSceneAtlasFrameResolver.Frame frame = prepared.resolve(frameName);
        if (!validFrame(frame)) return null;
        return new Result(mode, frame);
    }

    private static boolean validFrame(GameSceneAtlasFrameResolver.Frame frame) {
        return frame != null &&
                frame.plistRelativePath != null && !frame.plistRelativePath.isEmpty() &&
                frame.textureRelativePath != null && !frame.textureRelativePath.isEmpty() &&
                frame.textureX >= 0 && frame.textureY >= 0 &&
                frame.textureWidth > 0 && frame.textureHeight > 0 &&
                frame.colorX >= 0 && frame.colorY >= 0 &&
                frame.colorWidth > 0 && frame.colorHeight > 0 &&
                frame.spriteWidth > 0 && frame.spriteHeight > 0 &&
                frame.sourceWidth > 0 && frame.sourceHeight > 0;
    }
}
