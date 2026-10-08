package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import java.io.File;

final class SingleLoginAtlasComposer {
    private static final String[] BACKGROUND_STACK = {
            "zjmbeijing.png",
            "zjmbeijing02.png",
            "zjmbeijing03.png"
    };

    private static final String[] LIGHT_STACK = {
            "zjmdengguang01.png",
            "zjmdengguang02.png",
            "zjmdengguang03.png",
            "zjmdengguang04.png"
    };

    // InitUI indices 6..10 map to these five frames; the recovered z-order
    // literal array assigns all of them z=4 and no action-chain is attached.
    private static final String[] BUILDING_STACK = {
            "zjmjianzhu01.png",
            "zjmjianzhu02.png",
            "zjmjianzhu03.png",
            "zjmjianzhu04.png",
            "zjmjianzhu05.png"
    };

    static final class AtlasLayer {
        final int width;
        final int height;
        final int left;
        final int top;
        final int sourceWidth;
        final int sourceHeight;
        final int[] pixels;

        AtlasLayer(
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
        final AtlasLayer[] backgrounds;
        final AtlasLayer[] buildings;
        final AtlasLayer[] lights;

        SceneAssets(AtlasLayer[] backgrounds, AtlasLayer[] buildings, AtlasLayer[] lights) {
            this.backgrounds = backgrounds;
            this.buildings = buildings;
            this.lights = lights;
        }
    }

    private SingleLoginAtlasComposer() {}

    static SceneAssets composeScene(File plistFile, File atlasFile) throws Exception {
        TexturePackerPlist.Frame[] backgroundFrames = readFrames(plistFile, BACKGROUND_STACK);
        TexturePackerPlist.Frame[] buildingFrames = readFrames(plistFile, BUILDING_STACK);
        TexturePackerPlist.Frame[] lightFrames = readFrames(plistFile, LIGHT_STACK);
        if (backgroundFrames == null || buildingFrames == null || lightFrames == null) return null;

        final int sourceWidth = backgroundFrames[0].sourceWidth;
        final int sourceHeight = backgroundFrames[0].sourceHeight;
        if (sourceWidth <= 0 || sourceHeight <= 0 ||
                ((long) sourceWidth * (long) sourceHeight) > 16_777_216L) {
            return null;
        }
        if (!sameSourceSize(backgroundFrames, sourceWidth, sourceHeight) ||
                !sameSourceSize(buildingFrames, sourceWidth, sourceHeight) ||
                !sameSourceSize(lightFrames, sourceWidth, sourceHeight)) {
            return null;
        }

        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        Bitmap atlas = BitmapFactory.decodeFile(atlasFile.getAbsolutePath(), options);
        if (atlas == null) return null;

        try {
            AtlasLayer[] backgrounds = extractLayers(backgroundFrames, atlas, sourceWidth, sourceHeight);
            AtlasLayer[] buildings = extractLayers(buildingFrames, atlas, sourceWidth, sourceHeight);
            AtlasLayer[] lights = extractLayers(lightFrames, atlas, sourceWidth, sourceHeight);
            if (backgrounds == null || buildings == null || lights == null) return null;
            return new SceneAssets(backgrounds, buildings, lights);
        } finally {
            atlas.recycle();
        }
    }

    private static AtlasLayer[] extractLayers(
            TexturePackerPlist.Frame[] frames,
            Bitmap atlas,
            int sourceWidth,
            int sourceHeight) {
        AtlasLayer[] layers = new AtlasLayer[frames.length];
        for (int index = 0; index < frames.length; index++) {
            TexturePackerPlist.Frame frame = frames[index];
            if (!insideAtlas(frame, atlas)) return null;

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
            layers[index] = new AtlasLayer(
                    frame.textureWidth,
                    frame.textureHeight,
                    left,
                    top,
                    sourceWidth,
                    sourceHeight,
                    pixels);
        }
        return layers;
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

    private static boolean valid(TexturePackerPlist.Frame frame) {
        return frame != null && !frame.rotated &&
                frame.textureWidth > 0 && frame.textureHeight > 0 &&
                frame.sourceWidth > 0 && frame.sourceHeight > 0;
    }
}
