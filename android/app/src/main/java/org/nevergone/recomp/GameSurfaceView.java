package org.nevergone.recomp;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.opengl.GLUtils;
import android.view.MotionEvent;

import java.io.File;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public final class GameSurfaceView extends GLSurfaceView implements GLSurfaceView.Renderer {
    private static final String LOGIN_BACKGROUND =
            "assets/Login/LoginScreenUI/LoginScreen_bj.png";

    private static native void nativeOnSurfaceCreated();
    private static native void nativeOnSurfaceChanged(int width, int height);
    private static native void nativeOnDrawFrame();
    private static native void nativeOnTouch(int action, int pointerId, float x, float y);
    private static native void nativeOnLoginTexture(int textureId, int width, int height);

    private final Context context;

    public GameSurfaceView(Context context) {
        super(context);
        this.context = context.getApplicationContext();
        setEGLContextClientVersion(2);
        setRenderer(this);
        setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
        setFocusableInTouchMode(true);
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        nativeOnSurfaceCreated();
        loadLoginBackgroundTexture();
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        nativeOnSurfaceChanged(width, height);
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        nativeOnDrawFrame();
    }

    public void reloadImportedResources() {
        queueEvent(this::loadLoginBackgroundTexture);
    }

    private void loadLoginBackgroundTexture() {
        File source = new File(context.getFilesDir(), LOGIN_BACKGROUND);
        Bitmap bitmap = BitmapFactory.decodeFile(source.getAbsolutePath());
        if (bitmap == null) {
            nativeOnLoginTexture(0, 0, 0);
            return;
        }

        int[] textureIds = new int[1];
        GLES20.glGenTextures(1, textureIds, 0);
        int textureId = textureIds[0];
        if (textureId == 0) {
            bitmap.recycle();
            nativeOnLoginTexture(0, 0, 0);
            return;
        }

        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
        GLES20.glTexParameteri(
                GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameteri(
                GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameteri(
                GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
        GLES20.glTexParameteri(
                GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
        GLUtils.texImage2D(GLES20.GL_TEXTURE_2D, 0, bitmap, 0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);

        nativeOnLoginTexture(textureId, bitmap.getWidth(), bitmap.getHeight());
        bitmap.recycle();
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
