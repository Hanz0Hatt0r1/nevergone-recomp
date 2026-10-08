package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Rect;

import java.io.File;

final class SingleLoginAtlasComposer {
    // Recovered from SingleLoginLayer::InitUI(): zjmbeijing.png is z=0,
    // while the dynamic zjmbeijing%02d loop creates 02 at z=2 and 03 at z=3.
    private static final String[] BASE_STACK = {
            "zjmbeijing.png",
            "zjmbeijing02.png",
            "zjmbeijing03.png"
    };

    // The next recovered animated group is created from zjmbeijing%02d loop
    // indices 2..5 and resolves to these four atlas frames. They remain small
    // trimmed textures; native rendering positions them in the shared source
    // coordinate system instead of allocating four 1136x640 bitmaps.
    private static final String[] LIGHT_STACK = {
            "zjmdengguang01.png",
            "zjmdengguang02.png",
            "zjmdengguang03.png",
            "zjmdengguang04.png"
    };

    static final class LightLayer {
        final int width;
        final int height;
        final int left;
        final int top;
        final int sourceWidth;
        final int sourceHeight;
        final int[] pixels;

        LightLayer(
                int width,
                int height,
                int left,
                int top,
                int sourceWidth,
                int sourceHeight,
                int[] pixels) {
            this.width = width;
            this.height = height;
            this.left = left;
            this.top = top;
            this.sourceWidth = sourceWidth;
            this.sourceHeight = sourceHeight;
            this.pixels = pixels;
        }
    }

    static final class SceneAssets {
        final Bitmap base;
        final LightLayer[] lights;

        SceneAssets(Bitmap base, LightLayer[] lights) {
            this.base = base;
            this.lights = lights;
        }
    }

    private SingleLoginAtlasComposer() {}

    static SceneAssets composeScene(File plistFile, File atlasFile) throws Exception {
        TexturePackerPlist.Frame[] baseFrames = readFrames(plistFile, BASE_STACK);
        TexturePackerPlist.Frame[] lightFrames = readFrames(plistFile, LIGHT_STACK);
        if (baseFrames == null || lightFrames == null) return null;

        final int sourceWidth = baseFrames[0].sourceWidth;
        final int sourceHeight = baseFrames[0].sourceHeight;
        if (sourceWidth <= 0 || sourceHeight <= 0 ||
                ((long) sourceWidth * (long) sourceHeight) > 16_777_216L) {
            return null;
        }
        if (!sameSourceSize(baseFrames, sourceWidth, sourceHeight) ||
                !sameSourceSize(lightFrames, sourceWidth, sourceHeight)) {
            return null;
        }

        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        Bitmap atlas = BitmapFactory.decodeFile(atlasFile.getAbsolutePath(), options);
        if (atlas == null) return null;

        Bitmap composite = Bitmap.createBitmap(sourceWidth, sourceHeight, Bitmap.Config.ARGB_8888);
        try {
            Canvas canvas = new Canvas(composite);
            for (TexturePackerPlist.Frame frame : baseFrames) {
                if (!insideAtlas(frame, atlas)) {
                    composite.recycle();
                    return null;
                }
                drawFrame(canvas, atlas, frame, sourceWidth, sourceHeight);
            }

            LightLayer[] lights = new LightLayer[lightFrames.length];
            for (int index = 0; index < lightFrames.length; index++) {
                TexturePackerPlist.Frame frame = lightFrames[index];
                if (!insideAtlas(frame, atlas)) {
                    composite.recycle();
                    return null;
                }
                int[] pixels = new int[frame.textureWidth * frame.textureHeight];
                atlas.getPixels(
                        pixels,
                        0,
                        frame.textureWidth,
                        frame.textureX,
                        frame.textureY,
                        frame.textureWidth,
                        frame.textureHeight);
                int left = (sourceWidth - frame.textureWidth) / 2 + frame.offsetX;
                int top = (sourceHeight - frame.textureHeight) / 2 - frame.offsetY;
                lights[index] = new LightLayer(
                        frame.textureWidth,
                        frame.textureHeight,
                        left,
                        top,
                        sourceWidth,
                        sourceHeight,
                        pixels);
            }
            return new SceneAssets(composite, lights);
        } finally {
            atlas.recycle();
        }
    }

    // Retained for small callers/tests that only need the static base stack.
    static Bitmap compose(File plistFile, File atlasFile) throws Exception {
        SceneAssets scene = composeScene(plistFile, atlasFile);
        return scene != null ? scene.base : null;
    }

    private static TexturePackerPlist.Frame[] readFrames(File plistFile, String[] names) throws Exception {
        TexturePackerPlist.Frame[] frames = new TexturePackerPlist.Frame[names.length];
        for (int index = 0; index < names.length; index++) {
            TexturePackerPlist.Frame frame = TexturePackerPlist.readFrame(plistFile, names[index]);
            if (!valid(frame)) return null;
            frames[index] = frame;
        }
        return frames;
    }

    private static boolean sameSourceSize(
            TexturePackerPlist.Frame[] frames, int sourceWidth, int sourceHeight) {
        for (TexturePackerPlist.Frame frame : frames) {
            if (frame.sourceWidth != sourceWidth || frame.sourceHeight != sourceHeight) return false;
        }
        return true;
    }

    private static boolean insideAtlas(TexturePackerPlist.Frame frame, Bitmap atlas) {
        return frame.textureX >= 0 && frame.textureY >= 0 &&
                frame.textureX + frame.textureWidth <= atlas.getWidth() &&
                frame.textureY + frame.textureHeight <= atlas.getHeight();
    }

    private static void drawFrame(
            Canvas canvas,
            Bitmap atlas,
            TexturePackerPlist.Frame frame,
            int sourceWidth,
            int sourceHeight) {
        int left = (sourceWidth - frame.textureWidth) / 2 + frame.offsetX;
        int top = (sourceHeight - frame.textureHeight) / 2 - frame.offsetY;
        Rect source = new Rect(
                frame.textureX,
                frame.textureY,
                frame.textureX + frame.textureWidth,
                frame.textureY + frame.textureHeight);
        Rect destination = new Rect(
                left,
                top,
                left + frame.textureWidth,
                top + frame.textureHeight);
        canvas.drawBitmap(atlas, source, destination, null);
    }

    private static boolean valid(TexturePackerPlist.Frame frame) {
        return frame != null && !frame.rotated &&
                frame.textureWidth > 0 && frame.textureHeight > 0 &&
                frame.sourceWidth > 0 && frame.sourceHeight > 0;
    }
}
