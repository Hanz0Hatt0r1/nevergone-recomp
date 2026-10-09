package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;

import java.io.File;
import java.util.Locale;

final class ChooseHeroActionControlLoader {
    private static final String[] BUTTON_FILES = {
            "Common/btn_standard_a.png",
            "Common/btn_standard_b.png",
            "Common/btn_standard_c.png"
    };
    private static final int FONT_SIZE = 24;

    private static native void nativeClear();
    private static native boolean nativeUploadButton(
            int index, int width, int height, int[] pixels);
    private static native boolean nativeUploadLabel(
            int index, int width, int height, int[] pixels);
    private static native String[] nativeControlStrings(
            String filesDir, int languageColumn);

    private ChooseHeroActionControlLoader() {}

    static boolean reloadFromFilesDir(String filesDir) {
        nativeClear();
        if (filesDir == null || filesDir.isEmpty()) return false;

        File assetRoot = new File(filesDir, "assets");
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        for (int index = 0; index < BUTTON_FILES.length; index++) {
            Bitmap bitmap = BitmapFactory.decodeFile(
                    new File(assetRoot, BUTTON_FILES[index]).getAbsolutePath(), options);
            if (bitmap == null || !uploadBitmap(index, bitmap, true)) {
                if (bitmap != null) bitmap.recycle();
                nativeClear();
                return false;
            }
            bitmap.recycle();
        }

        String[] labels = nativeControlStrings(filesDir, languageColumn(Locale.getDefault()));
        if (labels == null || labels.length != 2) {
            nativeClear();
            return false;
        }
        for (int index = 0; index < labels.length; index++) {
            Bitmap bitmap = renderCocosLabel(labels[index]);
            if (bitmap == null || !uploadBitmap(index, bitmap, false)) {
                if (bitmap != null) bitmap.recycle();
                nativeClear();
                return false;
            }
            bitmap.recycle();
        }
        return true;
    }

    private static boolean uploadBitmap(int index, Bitmap source, boolean button) {
        Bitmap bitmap = source;
        if (source.getConfig() != Bitmap.Config.ARGB_8888) {
            bitmap = source.copy(Bitmap.Config.ARGB_8888, false);
            if (bitmap == null) return false;
        }
        int width = bitmap.getWidth();
        int height = bitmap.getHeight();
        if (width <= 0 || height <= 0 || ((long) width * (long) height) > 16_777_216L) {
            if (bitmap != source) bitmap.recycle();
            return false;
        }
        int[] pixels = new int[width * height];
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
        boolean ok = button
                ? nativeUploadButton(index, width, height, pixels)
                : nativeUploadLabel(index, width, height, pixels);
        if (bitmap != source) bitmap.recycle();
        return ok;
    }

    private static Bitmap renderCocosLabel(String text) {
        if (text == null || text.isEmpty()) return null;
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

    private static int languageColumn(Locale locale) {
        if (locale == null) return 4;
        String language = locale.getLanguage();
        if ("zh".equalsIgnoreCase(language)) {
            String script = locale.getScript();
            String country = locale.getCountry();
            boolean traditional = "Hant".equalsIgnoreCase(script) ||
                    "TW".equalsIgnoreCase(country) ||
                    "HK".equalsIgnoreCase(country) ||
                    "MO".equalsIgnoreCase(country);
            return traditional ? 3 : 2;
        }
        if ("fr".equalsIgnoreCase(language)) return 6;
        if ("de".equalsIgnoreCase(language)) return 7;
        if ("ja".equalsIgnoreCase(language)) return 8;
        return 4;
    }
}
