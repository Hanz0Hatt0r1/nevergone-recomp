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

    private SingleLoginAtlasComposer() {}

    static Bitmap compose(File plistFile, File atlasFile) throws Exception {
        TexturePackerPlist.Frame[] frames = new TexturePackerPlist.Frame[BASE_STACK.length];
        int sourceWidth = 0;
        int sourceHeight = 0;
        for (int index = 0; index < BASE_STACK.length; index++) {
            TexturePackerPlist.Frame frame = TexturePackerPlist.readFrame(plistFile, BASE_STACK[index]);
            if (!valid(frame)) {
                return null;
            }
            if (index == 0) {
                sourceWidth = frame.sourceWidth;
                sourceHeight = frame.sourceHeight;
                if (((long) sourceWidth * (long) sourceHeight) > 16_777_216L) {
                    return null;
                }
            } else if (frame.sourceWidth != sourceWidth || frame.sourceHeight != sourceHeight) {
                return null;
            }
            frames[index] = frame;
        }

        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        Bitmap atlas = BitmapFactory.decodeFile(atlasFile.getAbsolutePath(), options);
        if (atlas == null) {
            return null;
        }

        Bitmap composite = Bitmap.createBitmap(sourceWidth, sourceHeight, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(composite);
        try {
            for (TexturePackerPlist.Frame frame : frames) {
                if (frame.textureX < 0 || frame.textureY < 0 ||
                        frame.textureX + frame.textureWidth > atlas.getWidth() ||
                        frame.textureY + frame.textureHeight > atlas.getHeight()) {
                    composite.recycle();
                    return null;
                }
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
        } finally {
            atlas.recycle();
        }
        return composite;
    }

    private static boolean valid(TexturePackerPlist.Frame frame) {
        return frame != null && !frame.rotated &&
                frame.textureWidth > 0 && frame.textureHeight > 0 &&
                frame.sourceWidth > 0 && frame.sourceHeight > 0;
    }
}
