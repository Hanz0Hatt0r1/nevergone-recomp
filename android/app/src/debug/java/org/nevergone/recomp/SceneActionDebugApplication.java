package org.nevergone.recomp;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;
import android.util.Log;
import android.view.Choreographer;
import android.view.View;
import android.view.ViewGroup;

import java.io.File;

/**
 * Debug-build-only explicit SceneActions device probe.
 *
 * This deliberately does not model the unrecovered native spatial trigger or PRNG ordering.
 * Supplying NEVERGONE_SCENE_ACTIONS_VARIANT through an adb intent is equivalent to saying
 * "the trigger has already been confirmed and this exact recovered variant was selected".
 */
public final class SceneActionDebugApplication extends Application
        implements Application.ActivityLifecycleCallbacks, Choreographer.FrameCallback {
    static final String EXTRA_VARIANT = "nevergone_scene_actions_variant";
    private static final String TAG = "NeverGoneSceneActionProbe";
    private static final long RETRY_NS = 1_000_000_000L;
    private static final float MAX_DT_SECONDS = 0.25f;

    private Activity resumedActivity;
    private volatile GameSurfaceView surface;
    private File assetRoot;
    private volatile int requestedVariant;
    private boolean callbackPosted;
    private volatile boolean started;
    private volatile boolean bornCompleted;
    private volatile long lastFrameNs;
    private volatile long lastStartAttemptNs;

    @Override
    public void onCreate() {
        super.onCreate();
        registerActivityLifecycleCallbacks(this);
    }

    @Override
    public void onActivityResumed(Activity activity) {
        if (!(activity instanceof MainActivity)) return;
        int variant = activity.getIntent().getIntExtra(EXTRA_VARIANT, 0);
        if (variant < 1 || variant > 4) {
            stopProbe(false);
            return;
        }

        resumedActivity = activity;
        requestedVariant = variant;
        assetRoot = new File(activity.getFilesDir(), "assets");
        surface = findSurface(activity.getWindow().getDecorView());
        started = false;
        bornCompleted = false;
        lastFrameNs = 0L;
        lastStartAttemptNs = 0L;
        if (surface == null) {
            Log.e(TAG, "probe requested but GameSurfaceView was not found");
            return;
        }
        Log.i(TAG, "explicit SceneActions probe enabled for variant " + variant);
        postFrameCallback();
    }

    @Override
    public void onActivityPaused(Activity activity) {
        if (activity != resumedActivity) return;
        stopProbe(true);
    }

    @Override
    public void onActivityDestroyed(Activity activity) {
        if (activity != resumedActivity) return;
        stopProbe(true);
    }

    @Override
    public void doFrame(long frameTimeNanos) {
        callbackPosted = false;
        final GameSurfaceView target = surface;
        final File root = assetRoot;
        final int variant = requestedVariant;
        if (target == null || root == null || variant < 1 || variant > 4 || resumedActivity == null) {
            return;
        }

        if (!started) {
            if (lastStartAttemptNs == 0L || frameTimeNanos - lastStartAttemptNs >= RETRY_NS) {
                lastStartAttemptNs = frameTimeNanos;
                target.queueEvent(() -> {
                    if (surface != target || requestedVariant != variant) return;
                    if (SceneActionLiveRuntime.start(root, variant)) {
                        started = true;
                        bornCompleted = false;
                        lastFrameNs = 0L;
                        Log.i(TAG, "SceneActions probe started for current-scene variant " + variant);
                    }
                });
            }
            postFrameCallback();
            return;
        }

        if (lastFrameNs == 0L) {
            lastFrameNs = frameTimeNanos;
            postFrameCallback();
            return;
        }
        long elapsedNs = frameTimeNanos - lastFrameNs;
        lastFrameNs = frameTimeNanos;
        if (elapsedNs <= 0L) {
            postFrameCallback();
            return;
        }
        float dt = Math.min(MAX_DT_SECONDS, elapsedNs / 1_000_000_000.0f);
        target.queueEvent(() -> tickOnGlThread(target, root, dt));
        postFrameCallback();
    }

    private void tickOnGlThread(GameSurfaceView target, File root, float dt) {
        if (surface != target || !started) return;

        if (!bornCompleted) {
            int born = SceneActionLiveRuntime.tickAndSubmit(
                    root, SceneActionLiveRuntime.SLOT_BORN, dt);
            if (born == SceneActionLiveRuntime.TICK_COMPLETED) {
                bornCompleted = true;
                Log.i(TAG, "born SceneActions slot completed");
            } else if (born < 0 || born == SceneActionLiveRuntime.TICK_REJECTED) {
                failProbe("born tick failed: " + born);
                return;
            }
        }

        int idle = SceneActionLiveRuntime.tickAndSubmit(
                root, SceneActionLiveRuntime.SLOT_IDLE, dt);
        if (idle == SceneActionLiveRuntime.TICK_LOOPED) {
            Log.d(TAG, "idle SceneActions slot looped to frame zero");
        } else if (idle < 0 || idle == SceneActionLiveRuntime.TICK_REJECTED ||
                idle == SceneActionLiveRuntime.TICK_COMPLETED) {
            failProbe("idle tick failed: " + idle);
        }
    }

    private void failProbe(String reason) {
        Log.e(TAG, reason);
        SceneActionLiveRuntime.reset();
        started = false;
        bornCompleted = false;
        lastFrameNs = 0L;
        // Keep the explicit request armed. A retry is useful after imported assets
        // or current-scene binding change while the debug activity remains resumed.
        lastStartAttemptNs = 0L;
    }

    private void stopProbe(boolean resetRuntime) {
        if (callbackPosted) {
            Choreographer.getInstance().removeFrameCallback(this);
            callbackPosted = false;
        }
        GameSurfaceView target = surface;
        if (resetRuntime && target != null) {
            target.queueEvent(SceneActionLiveRuntime::reset);
        }
        resumedActivity = null;
        surface = null;
        assetRoot = null;
        requestedVariant = 0;
        started = false;
        bornCompleted = false;
        lastFrameNs = 0L;
        lastStartAttemptNs = 0L;
    }

    private void postFrameCallback() {
        if (callbackPosted || surface == null || resumedActivity == null) return;
        callbackPosted = true;
        Choreographer.getInstance().postFrameCallback(this);
    }

    private static GameSurfaceView findSurface(View view) {
        if (view instanceof GameSurfaceView) return (GameSurfaceView) view;
        if (!(view instanceof ViewGroup)) return null;
        ViewGroup group = (ViewGroup) view;
        for (int index = 0; index < group.getChildCount(); ++index) {
            GameSurfaceView found = findSurface(group.getChildAt(index));
            if (found != null) return found;
        }
        return null;
    }

    @Override public void onActivityCreated(Activity activity, Bundle state) {}
    @Override public void onActivityStarted(Activity activity) {}
    @Override public void onActivityStopped(Activity activity) {}
    @Override public void onActivitySaveInstanceState(Activity activity, Bundle state) {}
}
