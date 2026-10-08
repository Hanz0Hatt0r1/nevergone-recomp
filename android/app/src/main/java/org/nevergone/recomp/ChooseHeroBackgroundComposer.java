package org.nevergone.recomp;

import android.graphics.Bitmap;

import java.io.File;

final class ChooseHeroBackgroundComposer {
    static final int DESIGN_WIDTH = 1136;
    static final int DESIGN_HEIGHT = 640;

    static final class SceneAssets {
        final TexturePackerAtlasExtractor.ExtractedFrame moon;
        final TexturePackerAtlasExtractor.ExtractedFrame moonMask;
        final TexturePackerAtlasExtractor.ExtractedFrame starField;
        final TexturePackerAtlasExtractor.ExtractedFrame moonBackground;
        final TexturePackerAtlasExtractor.ExtractedFrame moonGlow;
        final TexturePackerAtlasExtractor.ExtractedFrame stormBackground;
        final TexturePackerAtlasExtractor.ExtractedFrame groundLight;
        final TexturePackerAtlasExtractor.ExtractedFrame[] foregroundClouds;

        SceneAssets(
                TexturePackerAtlasExtractor.ExtractedFrame moon,
                TexturePackerAtlasExtractor.ExtractedFrame moonMask,
                TexturePackerAtlasExtractor.ExtractedFrame starField,
                TexturePackerAtlasExtractor.ExtractedFrame moonBackground,
                TexturePackerAtlasExtractor.ExtractedFrame moonGlow,
                TexturePackerAtlasExtractor.ExtractedFrame stormBackground,
                TexturePackerAtlasExtractor.ExtractedFrame groundLight,
                TexturePackerAtlasExtractor.ExtractedFrame[] foregroundClouds) {
            this.moon = moon;
            this.moonMask = moonMask;
            this.starField = starField;
            this.moonBackground = moonBackground;
            this.moonGlow = moonGlow;
            this.stormBackground = stormBackground;
            this.groundLight = groundLight;
            this.foregroundClouds = foregroundClouds;
        }
    }

    private ChooseHeroBackgroundComposer() {}

    static SceneAssets compose(
            File atlas01Plist,
            Bitmap atlas01,
            File atlas02Plist,
            Bitmap atlas02) throws Exception {
        if (atlas01Plist == null || atlas01 == null ||
                atlas02Plist == null || atlas02 == null) {
            return null;
        }

        TexturePackerAtlasExtractor.ExtractedFrame moon =
                extract(atlas01Plist, atlas01, "yueliang.png");
        TexturePackerAtlasExtractor.ExtractedFrame moonMask =
                extract(atlas02Plist, atlas02, "yueliangzhezhao.png");
        TexturePackerAtlasExtractor.ExtractedFrame starField =
                extract(atlas02Plist, atlas02, "xingkong.png");
        TexturePackerAtlasExtractor.ExtractedFrame moonBackground =
                extract(atlas01Plist, atlas01, "bejingyueliang.png");
        TexturePackerAtlasExtractor.ExtractedFrame moonGlow =
                extract(atlas01Plist, atlas01, "bejingyueliang01.png");
        TexturePackerAtlasExtractor.ExtractedFrame stormBackground =
                extract(atlas02Plist, atlas02, "bejingwuyun.png");
        TexturePackerAtlasExtractor.ExtractedFrame groundLight =
                extract(atlas01Plist, atlas01, "diguang.png");

        TexturePackerAtlasExtractor.ExtractedFrame[] foregroundClouds = {
                extract(atlas02Plist, atlas02, "qianjingyun01.png"),
                extract(atlas02Plist, atlas02, "qianjingyun02.png"),
                extract(atlas02Plist, atlas02, "qianjingyun03.png"),
        };

        if (!isDesignCanvasFrame(moon) ||
                !isDesignCanvasFrame(moonMask) ||
                !isDesignCanvasFrame(starField) ||
                !isExactDesignCanvasFrame(moonBackground) ||
                !isDesignCanvasFrame(moonGlow) ||
                !isExactDesignCanvasFrame(stormBackground) ||
                !isDesignCanvasFrame(groundLight)) {
            return null;
        }
        for (TexturePackerAtlasExtractor.ExtractedFrame cloud : foregroundClouds) {
            if (cloud == null) return null;
        }

        return new SceneAssets(
                moon,
                moonMask,
                starField,
                moonBackground,
                moonGlow,
                stormBackground,
                groundLight,
                foregroundClouds);
    }

    private static TexturePackerAtlasExtractor.ExtractedFrame extract(
            File plist,
            Bitmap atlas,
            String frameName) throws Exception {
        return TexturePackerAtlasExtractor.extract(
                TexturePackerPlist.readFrame(plist, frameName),
                atlas);
    }

    private static boolean isDesignCanvasFrame(
            TexturePackerAtlasExtractor.ExtractedFrame frame) {
        return frame != null &&
                frame.sourceWidth == DESIGN_WIDTH &&
                frame.sourceHeight == DESIGN_HEIGHT;
    }

    private static boolean isExactDesignCanvasFrame(
            TexturePackerAtlasExtractor.ExtractedFrame frame) {
        return isDesignCanvasFrame(frame) &&
                frame.width == DESIGN_WIDTH &&
                frame.height == DESIGN_HEIGHT &&
                frame.left == 0 &&
                frame.top == 0;
    }
}
