package org.nevergone.recomp;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Reconstructs the player SpriteFrameCache atlas order around one live
 * ActionFrameData body request.
 *
 * MEPlayer::createActions() preloads pNN_CC_01..09_default.plist before
 * MeActionsSystem::initWithFile() optionally adds ActionFrameData +0x60. The
 * shipped base assets can omit members of that logical family and can omit the
 * action-specific plist entirely, while already-preloaded frames remain usable.
 * Keep this bounded to the recovered names instead of scanning unrelated player
 * atlases.
 */
final class PlayerActionAtlasCacheResolver {
    private static final String ACTION_SUFFIX = "_default.plist";
    private static final int PRELOAD_FIRST = 1;
    private static final int PRELOAD_LAST = 9;

    private PlayerActionAtlasCacheResolver() {}

    static List<GameSceneAtlasResolver.Atlas> resolve(File assetRoot, String actionAtlasPlist) {
        if (assetRoot == null || actionAtlasPlist == null || actionAtlasPlist.isEmpty()) return null;

        String preloadStem = preloadStem(actionAtlasPlist);
        ArrayList<GameSceneAtlasResolver.Atlas> resolved = new ArrayList<>();

        if (preloadStem != null) {
            for (int index = PRELOAD_FIRST; index <= PRELOAD_LAST; ++index) {
                String name = preloadStem + "_0" + index + ACTION_SUFFIX;
                File existing = GameSceneAtlasResolver.resolveResource(assetRoot, name);
                if (existing == null) continue;

                List<GameSceneAtlasResolver.Atlas> one = GameSceneAtlasResolver.resolve(
                        assetRoot, Collections.singletonList(name));
                if (one == null || one.size() != 1) return null;
                resolved.add(one.get(0));
            }
        }

        File exact = GameSceneAtlasResolver.resolveResource(assetRoot, actionAtlasPlist);
        if (exact != null) {
            List<GameSceneAtlasResolver.Atlas> one = GameSceneAtlasResolver.resolve(
                    assetRoot, Collections.singletonList(actionAtlasPlist));
            if (one == null || one.size() != 1) return null;
            resolved.add(one.get(0));
        } else if (preloadStem == null) {
            // Only the pNN_CC_default.plist shape has evidence-backed preload
            // semantics. Other action plist names remain exact-only.
            return null;
        }

        return resolved.isEmpty() ? null : Collections.unmodifiableList(resolved);
    }

    private static String preloadStem(String actionAtlasPlist) {
        if (actionAtlasPlist.indexOf('/') >= 0 || actionAtlasPlist.indexOf('\\') >= 0 ||
                actionAtlasPlist.indexOf('\0') >= 0 ||
                !actionAtlasPlist.startsWith("p") ||
                !actionAtlasPlist.endsWith(ACTION_SUFFIX)) {
            return null;
        }

        String stem = actionAtlasPlist.substring(
                0, actionAtlasPlist.length() - ACTION_SUFFIX.length());
        int separator = stem.indexOf('_', 1);
        if (separator <= 1 || separator >= stem.length() - 1 ||
                stem.indexOf('_', separator + 1) >= 0) {
            return null;
        }
        if (!digits(stem, 1, separator) || !digits(stem, separator + 1, stem.length())) {
            return null;
        }
        return stem;
    }

    private static boolean digits(String value, int start, int end) {
        if (start >= end) return false;
        for (int index = start; index < end; ++index) {
            char c = value.charAt(index);
            if (c < '0' || c > '9') return false;
        }
        return true;
    }
}
