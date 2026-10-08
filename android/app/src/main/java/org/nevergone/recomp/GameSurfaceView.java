package org.nevergone.recomp;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.opengl.GLSurfaceView;
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

    private static native void nativeOnSurfaceCreated();
    private static native void nativeOnSurfaceChanged(int width, int height);
    private static native void nativeOnDrawFrame();
    private static native void nativeOnTouch(int action, int pointerId, float x, float y);

    // Existing single-texture path remains as a graceful fallback when an
    // incomplete original asset set is imported.
    private static native boolean nativeUploadSplashTexture(int width, int height, int[] argbPixels);
    private static native void nativeClearSplashTexture();

    private static native void nativeOnSplashSurfaceCreated();
    private static native void nativeClearSplashFrames();
    private static native boolean nativeUploadSplashFrame(
            int frameIndex, int width, int height, int[] argbPixels);
    private static native boolean nativeBeginSplashSequence();
    private static native void nativeDrawSplashLayers();

    private final File assetRoot;

    public GameSurfaceView(Context context) {
        super(context);
        assetRoot = new File(context.getFilesDir(), "assets");
        setEGLContextClientVersion(2);
        setRenderer(this);
        setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
        setFocusableInTouchMode(true);
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        nativeOnSurfaceCreated();
        nativeOnSplashSurfaceCreated();
        loadImportedSplashOnGlThread();
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        nativeOnSurfaceChanged(width, height);
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        nativeOnDrawFrame();
        nativeDrawSplashLayers();
    }

    public void reloadImportedSplash() {
        queueEvent(this::loadImportedSplashOnGlThread);
    }

    private void loadImportedSplashOnGlThread() {
        nativeClearSplashTexture();
        nativeClearSplashFrames();

        int loadedFrames = 0;
        for (int frameIndex = 0; frameIndex < SPLASH_FILES.length; frameIndex++) {
            File splash = findFile(assetRoot, SPLASH_FILES[frameIndex], 0);
            if (splash == null) {
                continue;
            }

            BitmapFactory.Options options = new BitmapFactory.Options();
            options.inPreferredConfig = Bitmap.Config.ARGB_8888;
            Bitmap decoded = BitmapFactory.decodeFile(splash.getAbsolutePath(), options);
            if (decoded == null) {
                continue;
            }

            Bitmap bitmap = decoded;
            if (decoded.getConfig() != Bitmap.Config.ARGB_8888) {
                Bitmap converted = decoded.copy(Bitmap.Config.ARGB_8888, false);
                decoded.recycle();
                if (converted == null) {
                    continue;
                }
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

            if (frameIndex == 0) {
                nativeUploadSplashTexture(width, height, pixels);
            }
            if (nativeUploadSplashFrame(frameIndex, width, height, pixels)) {
                loadedFrames++;
            }
        }

        // Only replace the already-stable single-texture fallback when the
        // complete verified six-layer sequence is resident and ready.
        if (loadedFrames == SPLASH_FILES.length && nativeBeginSplashSequence()) {
            nativeClearSplashTexture();
        }
    }

    private static File findFile(File directory, String targetName, int depth) {
        if (directory == null || !directory.isDirectory() || depth > 16) {
            return null;
        }
        File[] children = directory.listFiles();
        if (children == null) {
            return null;
        }
        for (File child : children) {
            if (child.isFile() && targetName.equalsIgnoreCase(child.getName())) {
                return child;
            }
        }
        for (File child : children) {
            if (child.isDirectory()) {
                File result = findFile(child, targetName, depth + 1);
                if (result != null) {
                    return result;
                }
            }
        }
        return null;
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_MOVE) {
            for (int index = 0; index < event.getPointerCount(); index++) {
                nativeOnTouch(
                        action,
                        event.getPointerId(index),
                        event.getX(index),
                        event.getY(index));
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
