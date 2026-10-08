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

    private static final String[] CLOUD_STACK = {
            "zjmyun01.png",
            "zjmyun02.png"
    };

    private static final String[] LIGHT_STACK = {
            "zjmdengguang01.png",
            "zjmdengguang02.png",
            "zjmdengguang03.png",
            "zjmdengguang04.png"
    };

    private static final String[] BUILDING_STACK = {
            "zjmjianzhu01.png",
            "zjmjianzhu02.png",
            "zjmjianzhu03.png",
            "zjmjianzhu04.png",
            "zjmjianzhu05.png"
    };

    private static final String[] SWAY_STACK = {
            "zjmqianjingshuzhi01.png",
            "zjmqianjingshuzhi02.png",
            "zjmqianjingshuzhi03.png",
            "ZJMyuanjing_tree_left01.png",
            "ZJMyuanjing_tree_left02.png",
            "ZJMyuanjing_tree_left03.png",
            "ZJMyuanjing_tree_left04.png",
            "ZJMyuanjing_tree_left05.png",
            "ZJMyuanjing_tree_left06.png"
    };

    private static final String[] LIGHTNING_STACK = {
            "zjmshandian01.png",
            "zjmshandian02.png",
            "zjmshandian03.png",
            "zjmshandian04.png",
            "zjmshandian05.png",
            "zjmshandian06.png",
            "zjmshandian07.png"
    };

    private static final String[] ILLUMINATION_STACK = {
            "zjmjianzhuzhaoliang01.png",
            "zjmjianzhuzhaoliang02.png",
            "zjmjianzhuzhaoliang03.png",
            "zjmjianzhuzhaoliang04.png"
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

    static final class AtlasTexture {
        final int width;
        final int height;
        final int[] pixels;

        AtlasTexture(int width, int height, int[] pixels) {
            this.width = width;
            this.height = height;
            this.pixels = pixels;
        }
    }

    static final class SceneAssets {
        final AtlasLayer[] backgrounds;
        final AtlasTexture[] clouds;
        final AtlasLayer[] buildings;
        final AtlasLayer[] lights;
        final AtlasLayer[] sways;
        final AtlasLayer[] lightning;
        final AtlasLayer[] illumination;

        SceneAssets(
                AtlasLayer[] backgrounds,
                AtlasTexture[] clouds,
                AtlasLayer[] buildings,
                AtlasLayer[] lights,
                AtlasLayer[] sways,
                AtlasLayer[] lightning,
                AtlasLayer[] illumination) {
            this.backgrounds = backgrounds;
            this.clouds = clouds;
            this.buildings = buildings;
            this.lights = lights;
            this.sways = sways;
            this.lightning = lightning;
            this.illumination = illumination;
        }
    }

    private SingleLoginAtlasComposer() {}

    static SceneAssets composeScene(File plistFile, File atlasFile) throws Exception {
        TexturePackerPlist.Frame[] backgroundFrames = readFrames(plistFile, BACKGROUND_STACK);
        TexturePackerPlist.Frame[] cloudFrames = readFrames(plistFile, CLOUD_STACK);
        TexturePackerPlist.Frame[] buildingFrames = readFrames(plistFile, BUILDING_STACK);
        TexturePackerPlist.Frame[] lightFrames = readFrames(plistFile, LIGHT_STACK);
        TexturePackerPlist.Frame[] swayFrames = readFrames(plistFile, SWAY_STACK);
        TexturePackerPlist.Frame[] lightningFrames = readFrames(plistFile, LIGHTNING_STACK);
        TexturePackerPlist.Frame[] illuminationFrames = readFrames(plistFile, ILLUMINATION_STACK);
        if (backgroundFrames == null || cloudFrames == null || buildingFrames == null ||
                lightFrames == null || swayFrames == null || lightningFrames == null ||
                illuminationFrames == null) return null;

        final int sourceWidth = backgroundFrames[0].sourceWidth;
        final int sourceHeight = backgroundFrames[0].sourceHeight;
        if (sourceWidth <= 0 || sourceHeight <= 0 ||
                ((long) sourceWidth * (long) sourceHeight) > 16_777_216L) {
            return null;
        }
        if (!sameSourceSize(backgroundFrames, sourceWidth, sourceHeight) ||
                !sameSourceSize(buildingFrames, sourceWidth, sourceHeight) ||
                !sameSourceSize(lightFrames, sourceWidth, sourceHeight) ||
                !sameSourceSize(lightningFrames, sourceWidth, sourceHeight) ||
                !sameSourceSize(illuminationFrames, sourceWidth, sourceHeight)) {
            return null;
        }

        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        Bitmap atlas = BitmapFactory.decodeFile(atlasFile.getAbsolutePath(), options);
        if (atlas == null) return null;

        try {
            AtlasLayer[] backgrounds = extractLayers(backgroundFrames, atlas, sourceWidth, sourceHeight);
            AtlasTexture[] clouds = extractTextures(cloudFrames, atlas);
            AtlasLayer[] buildings = extractLayers(buildingFrames, atlas, sourceWidth, sourceHeight);
            AtlasLayer[] lights = extractLayers(lightFrames, atlas, sourceWidth, sourceHeight);
            AtlasLayer[] sways = extractSpriteLayers(swayFrames, atlas);
            AtlasLayer[] lightning = extractLayers(lightningFrames, atlas, sourceWidth, sourceHeight);
            AtlasLayer[] illumination = extractLayers(illuminationFrames, atlas, sourceWidth, sourceHeight);
            if (backgrounds == null || clouds == null || buildings == null ||
                    lights == null || sways == null || lightning == null || illumination == null) return null;
            return new SceneAssets(
                    backgrounds, clouds, buildings, lights, sways, lightning, illumination);
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
            layers[index] = makeLayer(frame, atlas, sourceWidth, sourceHeight);
        }
        return layers;
    }

    private static AtlasLayer[] extractSpriteLayers(
            TexturePackerPlist.Frame[] frames, Bitmap atlas) {
        AtlasLayer[] layers = new AtlasLayer[frames.length];
        for (int index = 0; index < frames.length; index++) {
            TexturePackerPlist.Frame frame = frames[index];
            if (!insideAtlas(frame, atlas)) return null;
            layers[index] = makeLayer(frame, atlas, frame.sourceWidth, frame.sourceHeight);
        }
        return layers;
    }

    private static AtlasLayer makeLayer(
            TexturePackerPlist.Frame frame,
            Bitmap atlas,
            int sourceWidth,
            int sourceHeight) {
        int[] pixels = extractPixels(frame, atlas);
        int left = (sourceWidth - frame.textureWidth) / 2 + frame.offsetX;
        int top = (sourceHeight - frame.textureHeight) / 2 - frame.offsetY;
        return new AtlasLayer(
                frame.textureWidth,
                frame.textureHeight,
                left,
                top,
                sourceWidth,
                sourceHeight,
                pixels);
    }

    private static AtlasTexture[] extractTextures(TexturePackerPlist.Frame[] frames, Bitmap atlas) {
        AtlasTexture[] textures = new AtlasTexture[frames.length];
        for (int index = 0; index < frames.length; index++) {
            TexturePackerPlist.Frame frame = frames[index];
            if (!insideAtlas(frame, atlas) || frame.offsetX != 0 || frame.offsetY != 0 ||
                    frame.textureWidth != frame.sourceWidth || frame.textureHeight != frame.sourceHeight) {
                return null;
            }
            textures[index] = new AtlasTexture(
                    frame.textureWidth, frame.textureHeight, extractPixels(frame, atlas));
        }
        return textures;
    }

    private static int[] extractPixels(TexturePackerPlist.Frame frame, Bitmap atlas) {
        int[] pixels = new int[frame.textureWidth * frame.textureHeight];
        atlas.getPixels(
                pixels,
                0,
                frame.textureWidth,
                frame.textureX,
                frame.textureY,
                frame.textureWidth,
                frame.textureHeight);
        return pixels;
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
