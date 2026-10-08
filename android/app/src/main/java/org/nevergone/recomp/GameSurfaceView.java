package org.nevergone.recomp;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.opengl.GLSurfaceView;
import android.os.Handler;
import android.os.Looper;
import android.view.MotionEvent;

import java.io.File;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public final class GameSurfaceView extends GLSurfaceView implements GLSurfaceView.Renderer {
    private static final String[] SPLASH_FILES = {
            "HIPPIEGOLO01.png",
            "HIPPIEGOLO02.png",
            "HIPPIEGOLO03.png",
            "HIPPIEGOLO04.png",
            "HIPPIEGOLO05.png",
            "HIPPIEGOLO06.png"
    };
    private static final String SINGLE_LOGIN_DIR = "gamescene_ui/SingleLogin_UI";
    private static final String SINGLE_LOGIN_PLIST = "SingleLogin_default.plist";
    private static final String SINGLE_LOGIN_ATLAS = "SingleLogin_default.png";

    private static native void nativeOnSurfaceCreated();
    private static native void nativeOnSurfaceChanged(int width, int height);
    private static native void nativeOnDrawFrame();
    private static native void nativeOnTouch(int action, int pointerId, float x, float y);

    private static native boolean nativeUploadSplashTexture(int width, int height, int[] argbPixels);
    private static native void nativeClearSplashTexture();

    private static native void nativeOnSplashSurfaceCreated();
    private static native void nativeClearSplashFrames();
    private static native boolean nativeUploadSplashFrame(
            int frameIndex, int width, int height, int[] argbPixels);
    private static native boolean nativeBeginSplashSequence();
    private static native void nativeDrawSplashLayers();

    private static native void nativeResetRecoveredSceneSequence();
    private static native void nativeBeginRecoveredSceneSequence();

    private static native void nativeOnSingleLoginSurfaceCreated();
    private static native void nativeOnSingleLoginSurfaceChanged(int width, int height);
    private static native void nativeClearSingleLoginTexture();
    private static native boolean nativeUploadSingleLoginTexture(int width, int height, int[] argbPixels);
    private static native void nativeClearSingleLoginBuildings();
    private static native boolean nativeUploadSingleLoginBuilding(
            int buildingIndex,
            int width,
            int height,
            int left,
            int top,
            int sourceWidth,
            int sourceHeight,
            int[] argbPixels);
    private static native void nativeClearSingleLoginLights();
    private static native boolean nativeUploadSingleLoginLight(
            int lightIndex,
            int width,
            int height,
            int left,
            int top,
            int sourceWidth,
            int sourceHeight,
            int[] argbPixels);
    private static native void nativeDrawSingleLoginLayer();
    private static native boolean nativeIsSingleLoginActive();

    private final File assetRoot;
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final SingleLoginAudio singleLoginAudio;
    private boolean lastSingleLoginActive;

    public GameSurfaceView(Context context) {
        super(context);
        assetRoot = new File(context.getFilesDir(), "assets");
        singleLoginAudio = new SingleLoginAudio(assetRoot);
        setEGLContextClientVersion(2);
        setRenderer(this);
        setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
        setFocusableInTouchMode(true);
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        nativeOnSurfaceCreated();
        nativeOnSingleLoginSurfaceCreated();
        nativeOnSplashSurfaceCreated();
        reloadImportedVisualsOnGlThread();
        updateSingleLoginAudioStateOnGlThread();
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        nativeOnSurfaceChanged(width, height);
        nativeOnSingleLoginSurfaceChanged(width, height);
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        nativeOnDrawFrame();
        nativeDrawSingleLoginLayer();
        nativeDrawSplashLayers();
        updateSingleLoginAudioStateOnGlThread();
    }

    public void reloadImportedSplash() {
        queueEvent(() -> {
            reloadImportedVisualsOnGlThread();
            mainHandler.post(() -> {
                // A hot re-import restarts the recovered startup sequence, so
                // do not allow the previous SingleLogin BGM to leak under it.
                singleLoginAudio.setSceneActive(false);
                singleLoginAudio.onAssetsReloaded();
            });
        });
    }

    public void pauseImportedAudio() {
        mainHandler.post(singleLoginAudio::onPause);
    }

    public void resumeImportedAudio() {
        mainHandler.post(singleLoginAudio::onResume);
    }

    public void releaseImportedAudio() {
        mainHandler.post(singleLoginAudio::release);
    }

    public String importedAudioStatus() {
        return singleLoginAudio.status();
    }

    private void updateSingleLoginAudioStateOnGlThread() {
        boolean active = nativeIsSingleLoginActive();
        if (active == lastSingleLoginActive) {
            return;
        }
        lastSingleLoginActive = active;
        mainHandler.post(() -> singleLoginAudio.setSceneActive(active));
    }

    private void reloadImportedVisualsOnGlThread() {
        loadSingleLoginSceneOnGlThread();
        loadImportedSplashOnGlThread();
    }

    private void loadSingleLoginSceneOnGlThread() {
        nativeClearSingleLoginTexture();
        nativeClearSingleLoginBuildings();
        nativeClearSingleLoginLights();
        File directory = new File(assetRoot, SINGLE_LOGIN_DIR);
        File plist = new File(directory, SINGLE_LOGIN_PLIST);
        File atlasFile = new File(directory, SINGLE_LOGIN_ATLAS);
        if (!plist.isFile() || !atlasFile.isFile()) {
            return;
        }

        try {
            SingleLoginAtlasComposer.SceneAssets scene =
                    SingleLoginAtlasComposer.composeScene(plist, atlasFile);
            if (scene == null || scene.base == null) {
                return;
            }

            Bitmap composite = scene.base;
            int width = composite.getWidth();
            int height = composite.getHeight();
            int[] pixels = new int[width * height];
            composite.getPixels(pixels, 0, width, 0, 0, width, height);
            composite.recycle();
            if (!nativeUploadSingleLoginTexture(width, height, pixels)) {
                nativeClearSingleLoginBuildings();
                nativeClearSingleLoginLights();
                return;
            }

            for (int index = 0; index < scene.buildings.length; index++) {
                SingleLoginAtlasComposer.AtlasLayer building = scene.buildings[index];
                if (building == null || !nativeUploadSingleLoginBuilding(
                        index,
                        building.width,
                        building.height,
                        building.left,
                        building.top,
                        building.sourceWidth,
                        building.sourceHeight,
                        building.pixels)) {
                    nativeClearSingleLoginBuildings();
                    break;
                }
            }

            for (int index = 0; index < scene.lights.length; index++) {
                SingleLoginAtlasComposer.AtlasLayer light = scene.lights[index];
                if (light == null || !nativeUploadSingleLoginLight(
                        index,
                        light.width,
                        light.height,
                        light.left,
                        light.top,
                        light.sourceWidth,
                        light.sourceHeight,
                        light.pixels)) {
                    nativeClearSingleLoginLights();
                    break;
                }
            }
        } catch (Exception ignored) {
            nativeClearSingleLoginTexture();
            nativeClearSingleLoginBuildings();
            nativeClearSingleLoginLights();
        }
    }

    private void loadImportedSplashOnGlThread() {
        nativeResetRecoveredSceneSequence();
        nativeClearSplashTexture();
        nativeClearSplashFrames();

        int loadedFrames = 0;
        for (int frameIndex = 0; frameIndex < SPLASH_FILES.length; frameIndex++) {
            File splash = findFile(assetRoot, SPLASH_FILES[frameIndex], 0);
            if (splash == null) continue;

            BitmapFactory.Options options = new BitmapFactory.Options();
            options.inPreferredConfig = Bitmap.Config.ARGB_8888;
            Bitmap decoded = BitmapFactory.decodeFile(splash.getAbsolutePath(), options);
            if (decoded == null) continue;

            Bitmap bitmap = decoded;
            if (decoded.getConfig() != Bitmap.Config.ARGB_8888) {
                Bitmap converted = decoded.copy(Bitmap.Config.ARGB_8888, false);
                decoded.recycle();
                if (converted == null) continue;
                bitmap = converted;
            }

            int width = bitmap.getWidth();
            int height = bitmap.getHeight();
            if (width <= 0 || height <= 0 || ((long) width * (long) height) > 16_777_216L) {
                bitmap.recycle();
                continue;
            }

            int[] pixels = new int[width * height];
            bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
            bitmap.recycle();

            if (frameIndex == 0) nativeUploadSplashTexture(width, height, pixels);
            if (nativeUploadSplashFrame(frameIndex, width, height, pixels)) loadedFrames++;
        }

        if (loadedFrames == SPLASH_FILES.length && nativeBeginSplashSequence()) {
            // Keep the gameplay/UI and audio boundary on the same fixed-clock
            // tick as the actual GLES splash sequence start.
            nativeBeginRecoveredSceneSequence();
            nativeClearSplashTexture();
        }
    }

    private static File findFile(File directory, String targetName, int depth) {
        if (directory == null || !directory.isDirectory() || depth > 16) return null;
        File[] children = directory.listFiles();
        if (children == null) return null;
        for (File child : children) {
            if (child.isFile() && targetName.equalsIgnoreCase(child.getName())) return child;
        }
        for (File child : children) {
            if (child.isDirectory()) {
                File result = findFile(child, targetName, depth + 1);
                if (result != null) return result;
            }
        }
        return null;
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_MOVE) {
            for (int index = 0; index < event.getPointerCount(); index++) {
                nativeOnTouch(action, event.getPointerId(index), event.getX(index), event.getY(index));
            }
        } else {
            int actionIndex = event.getActionIndex();
            nativeOnTouch(
                    action,
                    event.getPointerId(actionIndex),
                    event.getX(actionIndex),
                    event.getY(actionIndex));
        }
        return true;
    }
}
