package org.nevergone.recomp;

import java.io.File;

/** Explicit Android driver for one staged/current-scene klhuo SceneActions pair. */
final class SceneActionLiveRuntime {
    static final int SLOT_BORN = SceneActionPairRuntimeLoader.SLOT_BORN;
    static final int SLOT_IDLE = SceneActionPairRuntimeLoader.SLOT_IDLE;

    // Native scene_actions_active_playback::TickStatus ordinals.
    static final int TICK_REJECTED = 0;
    static final int TICK_NO_ADVANCE = 1;
    static final int TICK_FRAME_CHANGED = 2;
    static final int TICK_COMPLETED = 3;
    static final int TICK_LOOPED = 4;

    // Java-only failure results; native tick statuses are always non-negative.
    static final int STEP_STAGE_FAILED = -1;
    static final int STEP_SUBMIT_FAILED = -2;

    // Native scene_actions_render_submit::SubmitStatus ordinals.
    private static final int SUBMIT_SUBMITTED = 0;

    // Native scene_actions_live_runtime::StartStatus ordinals.
    private static final int START_STARTED = 0;

    private SceneActionLiveRuntime() {}

    private static native int nativeStartCurrentScene(int variant);
    private static native int nativeTick(int slot, float dtSeconds);
    private static native int nativeCurrentFrameIndex(int slot);
    private static native boolean nativeReadyToSubmit(int slot);
    private static native int nativeSubmit(int slot);
    private static native int nativeCurrentVariant();
    private static native boolean nativeStarted();
    private static native void nativeReset();

    /**
     * Explicitly stages and fires the current-scene SceneActions trigger for one recovered variant.
     * No PRNG draw or trigger timing is inferred here; the caller owns both choices.
     */
    static boolean start(File assetRoot, int variant) {
        if (assetRoot == null || variant < 1 || variant > 4) return false;

        // Never mutate the retained document/pixel stores underneath an active
        // controller. Repeating the same explicit trigger is idempotent; asking
        // for another variant requires reset/recreated scene lifetime first.
        if (nativeStarted()) return nativeCurrentVariant() == variant;

        if (!SceneActionVisualRuntime.stageVariant(assetRoot, variant)) return false;

        int status = nativeStartCurrentScene(variant);
        if (status != START_STARTED) {
            SceneActionVisualRuntime.invalidate();
            return false;
        }

        // Validate both renderer-ready frames before mutating either compositor
        // submission. This makes malformed initial asset pairs fail as one unit.
        if (!nativeReadyToSubmit(SLOT_BORN) || !nativeReadyToSubmit(SLOT_IDLE)) {
            nativeReset();
            SceneActionVisualRuntime.invalidate();
            return false;
        }

        // Initial pixels were staged transactionally by SceneActionVisualRuntime.
        // Submit both retained children immediately, matching play()->updateData().
        if (nativeSubmit(SLOT_BORN) != SUBMIT_SUBMITTED ||
                nativeSubmit(SLOT_IDLE) != SUBMIT_SUBMITTED) {
            nativeReset();
            SceneActionVisualRuntime.invalidate();
            return false;
        }
        return true;
    }

    /**
     * Advances exactly one native slot using caller-supplied dt, stages the exact current frame
     * when necessary, and submits it to the GLES compositor. Completion deliberately performs no
     * new submit so the born sprite preserves its last display frame.
     */
    static int tickAndSubmit(File assetRoot, int slot, float dtSeconds) {
        if (assetRoot == null ||
                (slot != SLOT_BORN && slot != SLOT_IDLE) ||
                !Float.isFinite(dtSeconds) || dtSeconds < 0.0f ||
                !nativeStarted()) {
            return TICK_REJECTED;
        }

        int tick = nativeTick(slot, dtSeconds);
        if (tick == TICK_REJECTED || tick == TICK_COMPLETED) return tick;

        int frameIndex = nativeCurrentFrameIndex(slot);
        int variant = nativeCurrentVariant();
        if (frameIndex < 0 || variant < 1 || variant > 4) return TICK_REJECTED;

        if (!SceneActionCurrentFramePixelStager.stage(
                assetRoot, slot, variant, frameIndex)) {
            return STEP_STAGE_FAILED;
        }
        if (!nativeReadyToSubmit(slot)) return STEP_SUBMIT_FAILED;
        if (nativeSubmit(slot) != SUBMIT_SUBMITTED) return STEP_SUBMIT_FAILED;
        return tick;
    }

    static boolean started() {
        return nativeStarted();
    }

    static int currentVariant() {
        return nativeCurrentVariant();
    }

    static void reset() {
        nativeReset();
        SceneActionVisualRuntime.invalidate();
    }
}
