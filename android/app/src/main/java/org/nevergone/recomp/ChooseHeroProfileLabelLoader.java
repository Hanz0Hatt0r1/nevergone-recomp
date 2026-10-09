package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;

import java.io.File;
import java.util.Locale;

final class ChooseHeroProfileLabelLoader {
    private static final String BACKGROUND_PATH = "Login/ChooseHero/sel_hero_name_bg.png";
    private static final int FONT_SIZE = 20;

    private static native void nativeClear();
    private static native boolean nativeUploadBackground(
            int width, int height, int[] pixels);
    private static native boolean nativeUploadLabel(
            int slotId, int kind, int width, int height, int[] pixels);
    private static native String[] nativeProfileStrings(
            String filesDir, int slotId, int languageColumn);

    private ChooseHeroProfileLabelLoader() {}

    static boolean reloadFromFilesDir(String filesDir) {
        nativeClear();
        if (filesDir == null || filesDir.isEmpty()) return false;

        // These update-era Common button assets are optional and have their
        // own native store. Their absence must not make baseline profile labels
        // fail, so trigger the reload and intentionally ignore its result.
        ChooseHeroActionControlLoader.reloadFromFilesDir(filesDir);

        File backgroundFile = new File(new File(filesDir, "assets"), BACKGROUND_PATH);
        Bitmap background = BitmapFactory.decodeFile(backgroundFile.getAbsolutePath());
        if (background == null || !uploadBackground(background)) {
            if (background != null) background.recycle();
            nativeClear();
            return false;
        }
        background.recycle();

        final int languageColumn = languageColumn(Locale.getDefault());
        for (int slotId = 1; slotId <= 2; slotId++) {
            String[] text = nativeProfileStrings(filesDir, slotId, languageColumn);
            if (text == null) continue;
            if (text.length != 3) {
                nativeClear();
                return false;
            }
            for (int kind = 0; kind < text.length; kind++) {
                Bitmap label = renderCocosLabel(text[kind]);
                if (label == null || !uploadLabel(slotId, kind, label)) {
                    if (label != null) label.recycle();
                    nativeClear();
                    return false;
                }
                label.recycle();
            }
        }
        return true;
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
        // Shipped SystemLanguage::ReturnSystemLanguage falls back to the
        // English CSV column for English, Italian, Spanish, Russian, Korean,
        // unknown values, and out-of-range language enums.
        return 4;
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

    private static boolean uploadBackground(Bitmap bitmap) {
        int width = bitmap.getWidth();
        int height = bitmap.getHeight();
        if (width <= 0 || height <= 0 || ((long) width * (long) height) > 16_777_216L) {
            return false;
        }
        int[] pixels = new int[width * height];
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
        return nativeUploadBackground(width, height, pixels);
    }

    private static boolean uploadLabel(int slotId, int kind, Bitmap bitmap) {
        int width = bitmap.getWidth();
        int height = bitmap.getHeight();
        if (width <= 0 || height <= 0 || ((long) width * (long) height) > 16_777_216L) {
            return false;
        }
        int[] pixels = new int[width * height];
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
        return nativeUploadLabel(slotId, kind, width, height, pixels);
    }
}
