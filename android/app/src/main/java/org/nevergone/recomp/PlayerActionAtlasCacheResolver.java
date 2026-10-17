package org.nevergone.recomp;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;

/** Player cache preload and clothes-prefixed frame lookup recovered from native. */
final class PlayerActionAtlasCacheResolver {
    private PlayerActionAtlasCacheResolver() {}

    static List<GameSceneAtlasResolver.Atlas> resolve(
            File assetRoot, String actionAtlasPlist, int career, int clothesId) {
        if (assetRoot == null || actionAtlasPlist == null || actionAtlasPlist.isEmpty()) return null;
        ArrayList<GameSceneAtlasResolver.Atlas> resolved = new ArrayList<>();
        // MEPlayer +0x194 selects the preload family independently of the
        // current action's +0x60 plist. Never infer clothes from that filename.
        if (career >= 1 && career <= 5 && clothesId >= 0 && clothesId <= 65535) {
            for (int index = 1; index <= 9; ++index) {
                String name = String.format(Locale.ROOT,
                        "p%02d_%02d_%02d_default.plist", career, clothesId, index);
                if (GameSceneAtlasResolver.resolveResource(assetRoot, name) == null) continue;
                List<GameSceneAtlasResolver.Atlas> one = GameSceneAtlasResolver.resolve(
                        assetRoot, Collections.singletonList(name));
                if (one == null || one.size() != 1) return null;
                resolved.add(one.get(0));
            }
        }
        if (GameSceneAtlasResolver.resolveResource(assetRoot, actionAtlasPlist) != null) {
            List<GameSceneAtlasResolver.Atlas> one = GameSceneAtlasResolver.resolve(
                    assetRoot, Collections.singletonList(actionAtlasPlist));
            if (one == null || one.size() != 1) return null;
            resolved.add(one.get(0));
        }
        return resolved.isEmpty() ? null : Collections.unmodifiableList(resolved);
    }

    static GameSceneAtlasFrameResolver.Frame resolveFrame(
            GameSceneAtlasFrameResolver.Prepared cache, String name, int clothesId) {
        if (cache == null || name == null || name.isEmpty()) return null;
        // MeActionsSystem +0x30c: nonzero clothes first use "%02d%s";
        // a missing prefixed entry falls back to the unchanged action key.
        if (clothesId > 0 && clothesId <= 65535) {
            GameSceneAtlasFrameResolver.Frame frame = cache.resolve(
                    String.format(Locale.ROOT, "%02d%s", clothesId, name));
            if (frame != null) return frame;
        }
        return cache.resolve(name);
    }
}
