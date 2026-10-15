package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;

import org.w3c.dom.Document;
import org.w3c.dom.NodeList;

import java.io.File;
import java.util.Locale;

final class TutorialLessonOverlayAssetLoader {
    private static final String LESSON_DIR = "lessonsUI";
    private static final String KEYBOARD_NAME = "keyboard_lesson.png";
    private static final String LESSON_TEXT_NAME = "lesson_text_01.plist";
    private static final long MAX_PLIST_BYTES = 2L * 1024L * 1024L;
    private static final int FONT_SIZE = 25;
    private static final long MAX_PIXELS = 16_777_216L;

    private static native void nativeClear();
    private static native boolean nativeUploadKeyboard(
            int width, int height, int[] pixels);
    private static native boolean nativeUploadContinueLabel(
            int width, int height, int[] pixels);
    private static native String nativeLocalizedString(
            String filesDir, String key, int languageColumn);

    private TutorialLessonOverlayAssetLoader() {}

    static boolean reloadFromFilesDir(String filesDir) {
        nativeClear();
        if (filesDir == null || filesDir.isEmpty()) return false;

        File root = new File(filesDir, "assets");
        File lessonDir = new File(root, LESSON_DIR);
        File keyboardFile = new File(lessonDir, KEYBOARD_NAME);
        File lessonTextFile = new File(lessonDir, LESSON_TEXT_NAME);
        if (!keyboardFile.isFile() || !lessonTextFile.isFile() ||
                !hasRecoveredLessonKeys(lessonTextFile)) {
            return false;
        }

        Bitmap keyboard = decodeArgb(keyboardFile);
        if (keyboard == null) return false;

        String continueText = nativeLocalizedString(
                filesDir, "Continue", languageColumn(Locale.getDefault()));
        Bitmap continueLabel = renderCocosLabel(continueText);
        if (continueLabel == null) {
            keyboard.recycle();
            return false;
        }

        boolean keyboardUploaded = uploadKeyboard(keyboard);
        boolean labelUploaded = keyboardUploaded && uploadContinueLabel(continueLabel);
        keyboard.recycle();
        continueLabel.recycle();
        if (!labelUploaded) {
            nativeClear();
            return false;
        }
        return true;
    }

    private static boolean hasRecoveredLessonKeys(File plist) {
        try {
            Document document = ImportedPlistXml.parse(plist, MAX_PLIST_BYTES);
            if (document == null) return false;
            NodeList keys = document.getElementsByTagName("key");
            boolean textId = false;
            boolean textTitle = false;
            boolean text01 = false;
            for (int index = 0; index < keys.getLength(); index++) {
                String value = keys.item(index).getTextContent();
                if (value == null) continue;
                value = value.trim();
                if ("text_id".equals(value)) textId = true;
                else if ("text_title".equals(value)) textTitle = true;
                else if ("text_01".equals(value)) text01 = true;
            }
            return textId && textTitle && text01;
        } catch (Exception error) {
            return false;
        }
    }

    private static Bitmap decodeArgb(File file) {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        Bitmap decoded = BitmapFactory.decodeFile(file.getAbsolutePath(), options);
        if (decoded == null) return null;
        Bitmap bitmap = decoded;
        if (decoded.getConfig() != Bitmap.Config.ARGB_8888) {
            Bitmap converted = decoded.copy(Bitmap.Config.ARGB_8888, false);
            decoded.recycle();
            if (converted == null) return null;
            bitmap = converted;
        }
        int width = bitmap.getWidth();
        int height = bitmap.getHeight();
        if (width <= 0 || height <= 0 || (long) width * (long) height > MAX_PIXELS) {
            bitmap.recycle();
            return null;
        }
        return bitmap;
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
        if (width <= 0 || height <= 0 || (long) width * (long) height > MAX_PIXELS) {
            return null;
        }

        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        canvas.drawText(text, 0.0f, -metrics.top, paint);
        return bitmap;
    }

    private static boolean uploadKeyboard(Bitmap bitmap) {
        int width = bitmap.getWidth();
        int height = bitmap.getHeight();
        int[] pixels = new int[width * height];
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
        return nativeUploadKeyboard(width, height, pixels);
    }

    private static boolean uploadContinueLabel(Bitmap bitmap) {
        int width = bitmap.getWidth();
        int height = bitmap.getHeight();
        int[] pixels = new int[width * height];
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
        return nativeUploadContinueLabel(width, height, pixels);
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
