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
    private static final String SINGLE_SELECT_HERO_DIR = SINGLE_LOGIN_DIR + "/SingleSelectHero";
    private static final String SINGLE_SELECT_HERO_PLIST = "Singleselechero.plist";
    private static final String SINGLE_SELECT_HERO_ATLAS = "Singleselechero.png";
    private static final String CHOOSE_HERO_BACKGROUND_DIR =
            "gamescene_ui/LevelUI/Gate_Background_UI";
    private static final String CHOOSE_HERO_PLIST_01 = "Gate_BackgroundPNG_01.plist";
    private static final String CHOOSE_HERO_ATLAS_01 = "Gate_BackgroundPNG_01.png";
    private static final String CHOOSE_HERO_PLIST_02 = "Gate_BackgroundPNG_02.plist";
    private static final String CHOOSE_HERO_ATLAS_02 = "Gate_BackgroundPNG_02.png";

    private static native void nativeOnSurfaceCreated();
    private static native void nativeOnSurfaceChanged(int width, int height);
    private static native void nativeOnDrawFrame();
    private static native void nativeOnTouch(int action, int pointerId, float x, float y);
    private static native void nativeOnGameSceneDirectTexturesSurfaceCreated();
    private static native boolean nativeSyncGameSceneDirectTextures();

    private static native boolean nativeUploadSplashTexture(int width, int height, int[] argbPixels);
    private static native void nativeClearSplashTexture();

    private static native void nativeOnSplashSurfaceCreated();
    private static native void nativeClearSplashFrames();
    private static native boolean nativeUploadSplashFrame(
            int frameIndex, int width, int height, int[] argbPixels);
    private static native boolean nativeBeginSplashSequence();
    private static native void nativeDrawSplashLayers();
    private static native boolean nativeIsSplashSoundDue();

    private static native void nativeResetRecoveredSceneSequence();
    private static native void nativeBeginRecoveredSceneSequence();

    private static native void nativeOnSingleLoginSurfaceCreated();
    private static native void nativeOnSingleLoginSurfaceChanged(int width, int height);
    private static native void nativeClearSingleLoginBackgrounds();
    private static native boolean nativeUploadSingleLoginBackground(
            int backgroundIndex,
            int width,
            int height,
            int left,
            int top,
            int sourceWidth,
            int sourceHeight,
            int[] argbPixels);
    private static native void nativeClearSingleLoginClouds();
    private static native boolean nativeUploadSingleLoginCloud(
            int cloudFrameIndex, int width, int height, int[] argbPixels);
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
    private static native void nativeClearSingleLoginSways();
    private static native boolean nativeUploadSingleLoginSway(
            int swayIndex,
            int width,
            int height,
            int left,
            int top,
            int sourceWidth,
            int sourceHeight,
            int[] argbPixels);
    private static native void nativeClearSingleLoginLightning();
    private static native boolean nativeUploadSingleLoginLightning(
            int lightningIndex,
            int width,
            int height,
            int left,
            int top,
            int sourceWidth,
            int sourceHeight,
            int[] argbPixels);
    private static native void nativeClearSingleLoginIllumination();
    private static native boolean nativeUploadSingleLoginIllumination(
            int illuminationIndex,
            int width,
            int height,
            int left,
            int top,
            int sourceWidth,
            int sourceHeight,
            int[] argbPixels);
    private static native void nativeDrawSingleLoginLayer();
    private static native boolean nativeIsSingleLoginActive();
    private static native int nativePollSingleLoginThunderSound();

    private static native void nativeOnServerSelectionSurfaceCreated();
    private static native void nativeOnServerSelectionSurfaceChanged(int width, int height);
    private static native void nativeDrawServerSelectionLayer();
    private static native boolean nativeOnServerSelectionTouch(
            int action, int pointerId, float x, float y);

    private static native void nativeOnSingleSelectHeroSurfaceCreated();
    private static native void nativeOnSingleSelectHeroSurfaceChanged(int width, int height);
    private static native void nativeClearSingleSelectHeroBackground();
    private static native boolean nativeUploadSingleSelectHeroBackground(
            int width,
            int height,
            int left,
            int top,
            int sourceWidth,
            int sourceHeight,
            int[] argbPixels);
    private static native void nativeDrawSingleSelectHeroLayer();
    private static native boolean nativeIsSingleSelectHeroActive();

    private static native void nativeClearChooseHeroBackgroundAssets();
    private static native boolean nativeUploadChooseHeroBackgroundAsset(
            int frameIndex,
            int width,
            int height,
            int left,
            int top,
            int sourceWidth,
            int sourceHeight,
            int[] argbPixels);
    private static native boolean nativeChooseHeroBackgroundAssetsReady();
    private static native boolean nativeIsChooseHeroRouteActive();

    private final File assetRoot;
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final StartupLogoAudio startupLogoAudio;
    private final SingleLoginAudio singleLoginAudio;
    private final SingleLoginThunderAudio singleLoginThunderAudio;
    private final SingleSelectHeroAudio singleSelectHeroAudio;
    private boolean lastSplashSoundDue;
    private boolean lastSingleLoginActive;
    private boolean lastSingleSelectHeroActive;
    private boolean chooseHeroBackgroundAssetsLoaded;

    public GameSurfaceView(Context context) {
        super(context);
        assetRoot = new File(context.getFilesDir(), "assets");
        startupLogoAudio = new StartupLogoAudio(assetRoot);
        singleLoginAudio = new SingleLoginAudio(assetRoot);
        singleLoginThunderAudio = new SingleLoginThunderAudio(assetRoot);
        singleSelectHeroAudio = new SingleSelectHeroAudio(assetRoot);
        setEGLContextClientVersion(2);
        setRenderer(this);
        setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
        setFocusableInTouchMode(true);
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        GameSceneDirectAssetStager.resetFailure();
        nativeOnSurfaceCreated();
        nativeOnGameSceneDirectTexturesSurfaceCreated();
        nativeOnSingleLoginSurfaceCreated();
        nativeOnServerSelectionSurfaceCreated();
        nativeOnSingleSelectHeroSurfaceCreated();
        nativeOnSplashSurfaceCreated();
        reloadImportedVisualsOnGlThread();
        updateGameSceneDirectAssetsOnGlThread();
        updateImportedAudioStateOnGlThread();
        updateChooseHeroBackgroundAssetsOnGlThread();
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        nativeOnSurfaceChanged(width, height);
        nativeOnSingleLoginSurfaceChanged(width, height);
        nativeOnServerSelectionSurfaceChanged(width, height);
        nativeOnSingleSelectHeroSurfaceChanged(width, height);
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        updateGameSceneDirectAssetsOnGlThread();
        nativeOnDrawFrame();
        nativeDrawSingleLoginLayer();
        nativeDrawSingleSelectHeroLayer();
        nativeDrawSplashLayers();
        nativeDrawServerSelectionLayer();
        updateImportedAudioStateOnGlThread();
        updateChooseHeroBackgroundAssetsOnGlThread();
    }

    public void reloadImportedSplash() {
        queueEvent(() -> {
            lastSplashSoundDue = false;
            lastSingleLoginActive = false;
            lastSingleSelectHeroActive = false;
            chooseHeroBackgroundAssetsLoaded = false;
            nativeClearChooseHeroBackgroundAssets();
            GameSceneDirectAssetStager.invalidate();
            mainHandler.post(() -> {
                startupLogoAudio.resetSequence();
                singleLoginAudio.setSceneActive(false);
                singleLoginThunderAudio.setSceneActive(false);
                singleSelectHeroAudio.setSceneActive(false);
            });
            reloadImportedVisualsOnGlThread();
            updateGameSceneDirectAssetsOnGlThread();
            mainHandler.post(() -> {
                startupLogoAudio.onAssetsReloaded();
                singleLoginAudio.onAssetsReloaded();
                singleLoginThunderAudio.onAssetsReloaded();
                singleSelectHeroAudio.onAssetsReloaded();
            });
        });
    }

    public void pauseImportedAudio() {
        mainHandler.post(() -> {
            startupLogoAudio.onPause();
            singleLoginAudio.onPause();
            singleLoginThunderAudio.onPause();
            singleSelectHeroAudio.onPause();
        });
    }

    public void resumeImportedAudio() {
        mainHandler.post(() -> {
            startupLogoAudio.onResume();
            singleLoginAudio.onResume();
            singleLoginThunderAudio.onResume();
            singleSelectHeroAudio.onResume();
        });
    }

    public void releaseImportedAudio() {
        mainHandler.post(() -> {
            startupLogoAudio.release();
            singleLoginAudio.release();
            singleLoginThunderAudio.release();
            singleSelectHeroAudio.release();
        });
    }

    public String importedAudioStatus() {
        return "Splash SFX: " + startupLogoAudio.status() +
                "\nSingleLogin BGM: " + singleLoginAudio.status() +
                "\nSingleLogin thunder: " + singleLoginThunderAudio.status() +
                "\nSingleSelectHero BGM: " + singleSelectHeroAudio.status();
    }

    private void updateGameSceneDirectAssetsOnGlThread() {
        GameSceneDirectAssetStager.stageIfNeeded(assetRoot);
        nativeSyncGameSceneDirectTextures();
    }

    private void updateImportedAudioStateOnGlThread() {
        boolean splashSoundDue = nativeIsSplashSoundDue();
        if (splashSoundDue != lastSplashSoundDue) {
            lastSplashSoundDue = splashSoundDue;
            mainHandler.post(() -> startupLogoAudio.setDue(splashSoundDue));
        }

        boolean singleLoginActive = nativeIsSingleLoginActive();
        if (singleLoginActive != lastSingleLoginActive) {
            lastSingleLoginActive = singleLoginActive;
            mainHandler.post(() -> {
                singleLoginAudio.setSceneActive(singleLoginActive);
                singleLoginThunderAudio.setSceneActive(singleLoginActive);
            });
        }

        boolean singleSelectHeroActive = nativeIsSingleSelectHeroActive();
        if (singleSelectHeroActive != lastSingleSelectHeroActive) {
            lastSingleSelectHeroActive = singleSelectHeroActive;
            mainHandler.post(() -> singleSelectHeroAudio.setSceneActive(singleSelectHeroActive));
        }

        for (int soundIndex = nativePollSingleLoginThunderSound();
                soundIndex >= 0;
                soundIndex = nativePollSingleLoginThunderSound()) {
            final int queuedSound = soundIndex;
            mainHandler.post(() -> singleLoginThunderAudio.play(queuedSound));
        }
    }

    private void updateChooseHeroBackgroundAssetsOnGlThread() {
        boolean routeActive = nativeIsChooseHeroRouteActive();
        if (!routeActive) {
            if (chooseHeroBackgroundAssetsLoaded) {
                nativeClearChooseHeroBackgroundAssets();
                chooseHeroBackgroundAssetsLoaded = false;
            }
            return;
        }
        if (chooseHeroBackgroundAssetsLoaded) return;
        loadChooseHeroBackgroundAssetsOnGlThread();
    }

    private void reloadImportedVisualsOnGlThread() {
        nativeClearChooseHeroBackgroundAssets();
        chooseHeroBackgroundAssetsLoaded = false;
        loadSingleLoginSceneOnGlThread();
        loadSingleSelectHeroBaseOnGlThread();
        loadImportedSplashOnGlThread();
    }

    private void clearSingleLoginSceneOnGlThread() {
        nativeClearSingleLoginBackgrounds();
        nativeClearSingleLoginClouds();
        nativeClearSingleLoginBuildings();
        nativeClearSingleLoginLights();
        nativeClearSingleLoginSways();
        nativeClearSingleLoginLightning();
        nativeClearSingleLoginIllumination();
    }

    private void loadSingleLoginSceneOnGlThread() {
        clearSingleLoginSceneOnGlThread();
        File directory = new File(assetRoot, SINGLE_LOGIN_DIR);
        File plist = new File(directory, SINGLE_LOGIN_PLIST);
        File atlasFile = new File(directory, SINGLE_LOGIN_ATLAS);
        if (!plist.isFile() || !atlasFile.isFile()) {
            return;
        }

        try {
            SingleLoginAtlasComposer.SceneAssets scene =
                    SingleLoginAtlasComposer.composeScene(plist, atlasFile);
            if (scene == null || scene.backgrounds == null || scene.backgrounds.length != 3 ||
                    scene.clouds == null || scene.clouds.length != 2 ||
                    scene.sways == null || scene.sways.length != 9 ||
                    scene.lightning == null || scene.lightning.length != 7 ||
                    scene.illumination == null || scene.illumination.length != 4) {
                return;
            }

            for (int index = 0; index < scene.backgrounds.length; index++) {
                SingleLoginAtlasComposer.AtlasLayer background = scene.backgrounds[index];
                if (background == null || !nativeUploadSingleLoginBackground(
                        index,
                        background.width,
                        background.height,
                        background.left,
                        background.top,
                        background.sourceWidth,
                        background.sourceHeight,
                        background.pixels)) {
                    clearSingleLoginSceneOnGlThread();
                    return;
                }
            }

            for (int index = 0; index < scene.clouds.length; index++) {
                SingleLoginAtlasComposer.AtlasTexture cloud = scene.clouds[index];
                if (cloud == null || !nativeUploadSingleLoginCloud(
                        index, cloud.width, cloud.height, cloud.pixels)) {
                    clearSingleLoginSceneOnGlThread();
                    return;
                }
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
                    clearSingleLoginSceneOnGlThread();
                    return;
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
                    clearSingleLoginSceneOnGlThread();
                    return;
                }
            }

            for (int index = 0; index < scene.sways.length; index++) {
                SingleLoginAtlasComposer.AtlasLayer sway = scene.sways[index];
                if (sway == null || !nativeUploadSingleLoginSway(
                        index,
                        sway.width,
                        sway.height,
                        sway.left,
                        sway.top,
                        sway.sourceWidth,
                        sway.sourceHeight,
                        sway.pixels)) {
                    clearSingleLoginSceneOnGlThread();
                    return;
                }
            }

            for (int index = 0; index < scene.lightning.length; index++) {
                SingleLoginAtlasComposer.AtlasLayer lightning = scene.lightning[index];
                if (lightning == null || !nativeUploadSingleLoginLightning(
                        index,
                        lightning.width,
                        lightning.height,
                        lightning.left,
                        lightning.top,
                        lightning.sourceWidth,
                        lightning.sourceHeight,
                        lightning.pixels)) {
                    clearSingleLoginSceneOnGlThread();
                    return;
                }
            }

            for (int index = 0; index < scene.illumination.length; index++) {
                SingleLoginAtlasComposer.AtlasLayer illumination = scene.illumination[index];
                if (illumination == null || !nativeUploadSingleLoginIllumination(
                        index,
                        illumination.width,
                        illumination.height,
                        illumination.left,
                        illumination.top,
                        illumination.sourceWidth,
                        illumination.sourceHeight,
                        illumination.pixels)) {
                    clearSingleLoginSceneOnGlThread();
                    return;
                }
            }
        } catch (Exception ignored) {
            clearSingleLoginSceneOnGlThread();
        }
    }

    private void loadSingleSelectHeroBaseOnGlThread() {
        nativeClearSingleSelectHeroBackground();
        File directory = new File(assetRoot, SINGLE_SELECT_HERO_DIR);
        File plist = new File(directory, SINGLE_SELECT_HERO_PLIST);
        File atlasFile = new File(directory, SINGLE_SELECT_HERO_ATLAS);
        if (!plist.isFile() || !atlasFile.isFile()) return;

        try {
            SingleLoginAtlasComposer.AtlasLayer background =
                    SingleSelectHeroBaseComposer.composeBackground(plist, atlasFile);
            if (background == null || !nativeUploadSingleSelectHeroBackground(
                    background.width,
                    background.height,
                    background.left,
                    background.top,
                    background.sourceWidth,
                    background.sourceHeight,
                    background.pixels)) {
                nativeClearSingleSelectHeroBackground();
            }
        } catch (Exception ignored) {
            nativeClearSingleSelectHeroBackground();
        }
    }

    private void loadChooseHeroBackgroundAssetsOnGlThread() {
        nativeClearChooseHeroBackgroundAssets();
        File directory = new File(assetRoot, CHOOSE_HERO_BACKGROUND_DIR);
        File plist01 = new File(directory, CHOOSE_HERO_PLIST_01);
        File atlasFile01 = new File(directory, CHOOSE_HERO_ATLAS_01);
        File plist02 = new File(directory, CHOOSE_HERO_PLIST_02);
        File atlasFile02 = new File(directory, CHOOSE_HERO_ATLAS_02);
        if (!plist01.isFile() || !atlasFile01.isFile() ||
                !plist02.isFile() || !atlasFile02.isFile()) {
            return;
        }

        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        Bitmap atlas01 = BitmapFactory.decodeFile(atlasFile01.getAbsolutePath(), options);
        Bitmap atlas02 = BitmapFactory.decodeFile(atlasFile02.getAbsolutePath(), options);
        if (atlas01 == null || atlas02 == null) {
            if (atlas01 != null) atlas01.recycle();
            if (atlas02 != null) atlas02.recycle();
            return;
        }

        try {
            ChooseHeroBackgroundComposer.SceneAssets scene =
                    ChooseHeroBackgroundComposer.compose(plist01, atlas01, plist02, atlas02);
            if (scene == null || scene.foregroundClouds == null ||
                    scene.foregroundClouds.length != 3) {
                return;
            }

            TexturePackerAtlasExtractor.ExtractedFrame[] frames = {
                    scene.moon,
                    scene.moonMask,
                    scene.starField,
                    scene.moonBackground,
                    scene.moonGlow,
                    scene.stormBackground,
                    scene.groundLight,
                    scene.foregroundClouds[0],
                    scene.foregroundClouds[1],
                    scene.foregroundClouds[2],
            };
            for (int index = 0; index < frames.length; index++) {
                if (!uploadChooseHeroBackgroundFrame(index, frames[index])) {
                    nativeClearChooseHeroBackgroundAssets();
                    return;
                }
            }
            chooseHeroBackgroundAssetsLoaded = nativeChooseHeroBackgroundAssetsReady();
            if (!chooseHeroBackgroundAssetsLoaded) {
                nativeClearChooseHeroBackgroundAssets();
            }
        } catch (Exception ignored) {
            nativeClearChooseHeroBackgroundAssets();
        } finally {
            atlas01.recycle();
            atlas02.recycle();
        }
    }

    private static boolean uploadChooseHeroBackgroundFrame(
            int index,
            TexturePackerAtlasExtractor.ExtractedFrame frame) {
        return frame != null && nativeUploadChooseHeroBackgroundAsset(
                index,
                frame.width,
                frame.height,
                frame.left,
                frame.top,
                frame.sourceWidth,
                frame.sourceHeight,
                frame.pixels);
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
                boolean handled = nativeOnServerSelectionTouch(
                        action,
                        event.getPointerId(index),
                        event.getX(index),
                        event.getY(index));
                if (!handled) {
                    nativeOnTouch(
                            action,
                            event.getPointerId(index),
                            event.getX(index),
                            event.getY(index));
                }
            }
        } else {
            int actionIndex = event.getActionIndex();
            boolean handled = nativeOnServerSelectionTouch(
                    action,
                    event.getPointerId(actionIndex),
                    event.getX(actionIndex),
                    event.getY(actionIndex));
            if (!handled) {
                nativeOnTouch(
                        action,
                        event.getPointerId(actionIndex),
                        event.getX(actionIndex),
                        event.getY(actionIndex));
            }
        }
        return true;
    }
}
