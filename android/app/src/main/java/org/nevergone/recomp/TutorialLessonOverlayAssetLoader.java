package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

final class TutorialLessonOverlayAssetLoader {
    private static final String LESSON_DIR = "lessonsUI";
    private static final String KEYBOARD_NAME = "keyboard_lesson.png";
    private static final String LESSON_TEXT_NAME = "lesson_text_01.plist";
    private static final String LESSON_BOARD_NAME = "lesson_board.png";
    private static final String LESSON_TITLE_NAME = "lesson_title.png";
    private static final long MAX_PLIST_BYTES = 2L * 1024L * 1024L;
    private static final int FONT_SIZE = 25;
    private static final int PROMPT_TITLE_FONT_SIZE = 25;
    private static final int PROMPT_BODY_FONT_SIZE = 20;
    private static final int PROMPT_BODY_MAX_WIDTH = 309;
    private static final long MAX_PIXELS = 16_777_216L;

    private static native void nativeClear();
    private static native boolean nativeUploadKeyboard(
            int width, int height, int[] pixels);
    private static native boolean nativeUploadContinueLabel(
            int width, int height, int[] pixels);
    private static native void nativeClearPrompt();
    private static native boolean nativeUploadPromptBackgrounds(
            int boardWidth, int boardHeight, int[] boardPixels,
            int titlebarWidth, int titlebarHeight, int[] titlebarPixels);
    private static native boolean nativeUploadPromptText(
            int step,
            int titleWidth, int titleHeight, int[] titlePixels,
            int bodyWidth, int bodyHeight, int[] bodyPixels);
    private static native String nativeLocalizedString(
            String filesDir, String key, int languageColumn);

    private TutorialLessonOverlayAssetLoader() {}

    static boolean reloadFromFilesDir(String filesDir) {
        nativeClear();
        if (filesDir == null || filesDir.isEmpty()) return false;

        File root = new File(filesDir, "assets");
        File keyboardFile = GameSceneAtlasResolver.resolveResource(
                root, LESSON_DIR + "/" + KEYBOARD_NAME);
        File lessonTextFile = GameSceneAtlasResolver.resolveResource(
                root, LESSON_DIR + "/" + LESSON_TEXT_NAME);
        if (keyboardFile == null || lessonTextFile == null ||
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

    static boolean reloadPromptFromFilesDir(String filesDir, int tutorialStep) {
        nativeClearPrompt();
        if (filesDir == null || filesDir.isEmpty() || tutorialStep < 2 || tutorialStep > 10) {
            return false;
        }

        File root = new File(filesDir, "assets");
        File lessonTextFile = GameSceneAtlasResolver.resolveResource(
                root, LESSON_DIR + "/" + LESSON_TEXT_NAME);
        File boardFile = GameSceneAtlasResolver.resolveResource(
                root, LESSON_DIR + "/" + LESSON_BOARD_NAME);
        File titlebarFile = GameSceneAtlasResolver.resolveResource(
                root, LESSON_DIR + "/" + LESSON_TITLE_NAME);
        if (lessonTextFile == null || boardFile == null || titlebarFile == null) return false;

        String lessonTitleKey;
        String lessonBodyKey;
        try {
            Document document = ImportedPlistXml.parse(lessonTextFile, MAX_PLIST_BYTES);
            if (document == null) return false;
            NodeList dictionaries = document.getElementsByTagName("dict");
            if (dictionaries.getLength() == 0) return false;
            Element rootDictionary = (Element) dictionaries.item(0);
            int originalLessonStep = tutorialStep + 1;
            String entryName = String.format(Locale.ROOT, "dialogue_text_%02d", originalLessonStep);
            Element entry = childValue(rootDictionary, entryName, "dict");
            if (entry == null) return false;
            lessonTitleKey = stringValue(entry, "text_title");
            lessonBodyKey = stringValue(entry, "text_01");
        } catch (Exception error) {
            return false;
        }
        if (lessonTitleKey == null || lessonTitleKey.isEmpty() ||
                lessonBodyKey == null || lessonBodyKey.isEmpty()) return false;

        int language = languageColumn(Locale.getDefault());
        String titleText = nativeLocalizedString(filesDir, lessonTitleKey, language);
        String bodyText = nativeLocalizedString(filesDir, lessonBodyKey, language);
        if (titleText == null || bodyText == null || titleText.isEmpty() || bodyText.isEmpty()) {
            return false;
        }

        Bitmap board = decodeArgb(boardFile);
        Bitmap titlebar = decodeArgb(titlebarFile);
        Bitmap title = renderCocosLabel(titleText, PROMPT_TITLE_FONT_SIZE, 0);
        Bitmap body = renderCocosLabel(bodyText, PROMPT_BODY_FONT_SIZE, PROMPT_BODY_MAX_WIDTH);
        if (board == null || titlebar == null || title == null || body == null) {
            recycle(board);
            recycle(titlebar);
            recycle(title);
            recycle(body);
            return false;
        }

        boolean backgroundsUploaded = uploadPromptBackgrounds(board, titlebar);
        boolean promptUploaded = backgroundsUploaded && uploadPromptText(tutorialStep, title, body);
        recycle(board);
        recycle(titlebar);
        recycle(title);
        recycle(body);
        if (!promptUploaded) nativeClearPrompt();
        return promptUploaded;
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

    private static Element childValue(Element dictionary, String key, String expectedTag) {
        NodeList children = dictionary.getChildNodes();
        for (int index = 0; index < children.getLength(); index++) {
            Node node = children.item(index);
            if (node.getNodeType() != Node.ELEMENT_NODE || !"key".equals(node.getNodeName()) ||
                    !key.equals(node.getTextContent().trim())) continue;
            for (Node value = node.getNextSibling(); value != null; value = value.getNextSibling()) {
                if (value.getNodeType() != Node.ELEMENT_NODE) continue;
                return expectedTag.equals(value.getNodeName()) ? (Element) value : null;
            }
            return null;
        }
        return null;
    }

    private static String stringValue(Element dictionary, String key) {
        Element value = childValue(dictionary, key, "string");
        return value == null ? null : value.getTextContent().trim();
    }

    private static void recycle(Bitmap bitmap) {
        if (bitmap != null && !bitmap.isRecycled()) bitmap.recycle();
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
        return renderCocosLabel(text, FONT_SIZE, 0);
    }

    private static Bitmap renderCocosLabel(String text, int fontSize, int maxWidth) {
        if (text == null || text.isEmpty() || fontSize <= 0 || maxWidth < 0) return null;
        Paint paint = new Paint();
        paint.setColor(Color.WHITE);
        paint.setTextSize(fontSize);
        paint.setAntiAlias(true);
        paint.setTypeface(Typeface.create("Arial", Typeface.NORMAL));
        paint.setTextAlign(Paint.Align.LEFT);

        Paint.FontMetricsInt metrics = paint.getFontMetricsInt();
        List<String> lines = wrap(text, paint, maxWidth);
        if (lines.isEmpty()) return null;
        int width = maxWidth > 0
                ? maxWidth
                : (int) Math.ceil(paint.measureText(text, 0, text.length()));
        int lineHeight = (int) Math.ceil(metrics.bottom - metrics.top);
        int height = lineHeight * lines.size();
        if (width <= 0 || height <= 0 || (long) width * (long) height > MAX_PIXELS) {
            return null;
        }

        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        for (int index = 0; index < lines.size(); index++) {
            canvas.drawText(lines.get(index), 0.0f, -metrics.top + index * lineHeight, paint);
        }
        return bitmap;
    }

    private static List<String> wrap(String text, Paint paint, int maxWidth) {
        List<String> lines = new ArrayList<>();
        if (maxWidth <= 0) {
            lines.add(text);
            return lines;
        }
        for (String paragraph : text.split("\\R", -1)) {
            String line = "";
            for (String word : paragraph.trim().split("\\s+")) {
                if (word.isEmpty()) continue;
                String candidate = line.isEmpty() ? word : line + " " + word;
                if (paint.measureText(candidate) <= maxWidth) {
                    line = candidate;
                    continue;
                }
                if (!line.isEmpty()) lines.add(line);
                if (paint.measureText(word) <= maxWidth) {
                    line = word;
                    continue;
                }
                int offset = 0;
                while (offset < word.length()) {
                    int count = paint.breakText(word, offset, word.length(), true, maxWidth, null);
                    if (count <= 0) count = 1;
                    String part = word.substring(offset, offset + count);
                    if (offset + count < word.length()) lines.add(part);
                    else line = part;
                    offset += count;
                }
            }
            if (!line.isEmpty()) lines.add(line);
            else if (paragraph.isEmpty()) lines.add("");
        }
        return lines;
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

    private static boolean uploadPromptBackgrounds(Bitmap board, Bitmap titlebar) {
        int boardWidth = board.getWidth();
        int boardHeight = board.getHeight();
        int titlebarWidth = titlebar.getWidth();
        int titlebarHeight = titlebar.getHeight();
        int[] boardPixels = new int[boardWidth * boardHeight];
        int[] titlebarPixels = new int[titlebarWidth * titlebarHeight];
        board.getPixels(boardPixels, 0, boardWidth, 0, 0, boardWidth, boardHeight);
        titlebar.getPixels(titlebarPixels, 0, titlebarWidth, 0, 0, titlebarWidth, titlebarHeight);
        return nativeUploadPromptBackgrounds(
                boardWidth, boardHeight, boardPixels,
                titlebarWidth, titlebarHeight, titlebarPixels);
    }

    private static boolean uploadPromptText(int step, Bitmap title, Bitmap body) {
        int titleWidth = title.getWidth();
        int titleHeight = title.getHeight();
        int bodyWidth = body.getWidth();
        int bodyHeight = body.getHeight();
        int[] titlePixels = new int[titleWidth * titleHeight];
        int[] bodyPixels = new int[bodyWidth * bodyHeight];
        title.getPixels(titlePixels, 0, titleWidth, 0, 0, titleWidth, titleHeight);
        body.getPixels(bodyPixels, 0, bodyWidth, 0, 0, bodyWidth, bodyHeight);
        return nativeUploadPromptText(
                step,
                titleWidth, titleHeight, titlePixels,
                bodyWidth, bodyHeight, bodyPixels);
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
