package org.nevergone.recomp;

/**
 * Evidence-backed start point retained by one GameLevelsData scene entry.
 *
 * The original ARMv7 loader parses playerStartXPos/playerStartYPos through
 * CCString::floatValue(), passes those exact values to CCPoint(float, float),
 * then assigns the point into GameLevelsData at +0x34. This model preserves
 * only that proven pair; it does not imply when or how a player node consumes it.
 */
final class GameSceneStartPoint {
    final float x;
    final float y;

    private GameSceneStartPoint(float x, float y) {
        this.x = x;
        this.y = y;
    }

    static GameSceneStartPoint of(float x, float y) {
        if (!Float.isFinite(x) || !Float.isFinite(y)) {
            throw new IllegalArgumentException("GameScene start point must be finite");
        }
        return new GameSceneStartPoint(x, y);
    }
}
