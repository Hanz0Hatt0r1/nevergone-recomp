package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.util.Xml;

import org.xmlpull.v1.XmlPullParser;

import java.io.File;
import java.io.FileInputStream;

final class ServerSelectionAssetLoader {
    private static final String[] FILES = {
            "gamescene_ui/ServerList/border1.png",
            "gamescene_ui/ServerList/border2.png",
            "Common/btn_standard_a.png",
            "Common/btn_standard_b.png",
            "Common/btn_standard_c.png"
    };
    private static final String STRINGS_FILE = "gamescene_ui/ServerList/XMLFile1.xml";
    private static final int START_LABEL_INDEX = 5;
    private static final int FONT_SIZE = 24;

    private static native void nativeClear();
    private static native boolean nativeUpload(
            int index, int width, int height, int[] pixels);

    private ServerSelectionAssetLoader() {}

    static boolean reloadFromFilesDir(String filesDir) {
        nativeClear();
        if (filesDir == null || filesDir.isEmpty()) return false;
        File assetRoot = new File(filesDir, "assets");
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        boolean uploadedAny = false;
        for (int index = 0; index < FILES.length; index++) {
            Bitmap decoded = BitmapFactory.decodeFile(
                    new File(assetRoot, FILES[index]).getAbsolutePath(), options);
            if (decoded == null) continue;
            try {
                if (!upload(index, decoded)) {
                    nativeClear();
                    return false;
                }
                uploadedAny = true;
            } finally {
                decoded.recycle();
            }
        }

        String startLabel = readString(new File(assetRoot, STRINGS_FILE), "start");
        if (startLabel != null && !startLabel.isEmpty()) {
            Bitmap label = renderLabel(startLabel);
            if (label != null) {
                try {
                    if (!upload(START_LABEL_INDEX, label)) {
                        nativeClear();
                        return false;
                    }
                    uploadedAny = true;
                } finally {
                    label.recycle();
                }
            }
        }
        return uploadedAny;
    }

    private static String readString(File file, String requestedKey) {
        if (file == null || requestedKey == null || !file.isFile()) return null;
        try (FileInputStream input = new FileInputStream(file)) {
            XmlPullParser parser = Xml.newPullParser();
            parser.setInput(input, "UTF-8");
            String key = null;
            int event = parser.getEventType();
            while (event != XmlPullParser.END_DOCUMENT) {
                if (event == XmlPullParser.START_TAG) {
                    String name = parser.getName();
                    if ("key".equals(name)) {
                        key = parser.nextText();
                    } else if ("string".equals(name)) {
                        String value = parser.nextText();
                        if (requestedKey.equals(key)) return value;
                        key = null;
                    }
                }
                event = parser.next();
            }
        } catch (Exception ignored) {
            return null;
        }
        return null;
    }

    private static Bitmap renderLabel(String text) {
        Paint paint = new Paint();
        paint.setColor(Color.WHITE);
        paint.setTextSize(FONT_SIZE);
        paint.setAntiAlias(true);
        paint.setTypeface(Typeface.create("Arial", Typeface.NORMAL));
        paint.setTextAlign(Paint.Align.LEFT);

        Paint.FontMetricsInt metrics = paint.getFontMetricsInt();
        int width = (int) Math.ceil(paint.measureText(text, 0, text.length()));
        int height = (int) Math.ceil(metrics.bottom - metrics.top);
        if (width <= 0 || height <= 0 || ((long) width * (long) height) > 16_777_216L) {
            return null;
        }
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        canvas.drawText(text, 0.0f, -metrics.top, paint);
        return bitmap;
    }

    private static boolean upload(int index, Bitmap source) {
        Bitmap bitmap = source;
        if (source.getConfig() != Bitmap.Config.ARGB_8888) {
            bitmap = source.copy(Bitmap.Config.ARGB_8888, false);
            if (bitmap == null) return false;
        }
        try {
            int width = bitmap.getWidth();
            int height = bitmap.getHeight();
            if (width <= 0 || height <= 0 || ((long) width * (long) height) > 16_777_216L) {
                return false;
            }
            int[] pixels = new int[width * height];
            bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
            return nativeUpload(index, width, height, pixels);
        } finally {
            if (bitmap != source) bitmap.recycle();
        }
    }
}
