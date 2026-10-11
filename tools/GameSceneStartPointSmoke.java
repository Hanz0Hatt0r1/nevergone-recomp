package org.nevergone.recomp;

import java.util.Collections;

public final class GameSceneStartPointSmoke {
    private GameSceneStartPointSmoke() {}

    public static void main(String[] args) {
        GameSceneAtlasListDiscovery.Result discovery =
                new GameSceneAtlasListDiscovery.Result(
                        "gamescene/list.plist",
                        Collections.singletonList("l01/res/scene.plist"),
                        123.5f,
                        -42.25f);
        GameSceneStartPoint point = GameSceneStartPoint.from(discovery);
        assert Float.compare(point.x, 123.5f) == 0;
        assert Float.compare(point.y, -42.25f) == 0;

        expectIllegalArgument(() -> GameSceneStartPoint.from(null));
        expectIllegalArgument(() -> GameSceneStartPoint.of(Float.NaN, 1.0f));
        expectIllegalArgument(() -> GameSceneStartPoint.of(1.0f, Float.POSITIVE_INFINITY));
    }

    private static void expectIllegalArgument(Runnable runnable) {
        boolean rejected = false;
        try {
            runnable.run();
        } catch (IllegalArgumentException expected) {
            rejected = true;
        }
        assert rejected;
    }
}
