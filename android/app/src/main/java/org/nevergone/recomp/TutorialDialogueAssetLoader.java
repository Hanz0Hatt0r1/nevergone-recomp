package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Typeface;
import android.text.Layout;
import android.text.StaticLayout;
import android.text.TextPaint;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;

import java.io.File;
import java.util.Locale;

/** Loads the first fresh-role DialogueUI sequence from user-imported assets. */
final class TutorialDialogueAssetLoader {
    private static final String GUIDE_PATH = "Guide/guide_text.plist";
    private static final String DIALOGUE_BACKGROUND = "Dialogue_UI/dialogue_bg.png";
    private static final int STARTUP_DIALOGUE_ID = 8;
    private static final String STARTUP_DIALOGUE_KEY = "dialogue_text_08";
    private static final int TEXT_WIDTH = 450;
    private static final int TEXT_SIZE = 20;
    private static final int MAX_PAGES = 64;
    private static final long MAX_PLIST_BYTES = 4L * 1024L * 1024L;
    private static final long MAX_PIXELS = 16_777_216L;

    private static native void nativeClear();
    private static native boolean nativeConfigure(
            int dialogueId, boolean autoPlay, int pageCount);
    private static native boolean nativeUploadBackground(
            int width, int height, int[] pixels);
    private static native boolean nativeUploadPage(
            int pageIndex, int width, int height, int[] pixels);
    private static native boolean nativeCommit();
    private static native String nativeLocalizedString(
            String filesDir, String key, int languageColumn);

    private TutorialDialogueAssetLoader() {}

    static boolean reloadFromFilesDir(String filesDir) {
        nativeClear();
        if (filesDir == null || filesDir.isEmpty()) return false;

        File root = new File(filesDir, "assets");
        File guideFile = new File(root, GUIDE_PATH);
        File backgroundFile = new File(root, DIALOGUE_BACKGROUND);
        if (!guideFile.isFile() || !backgroundFile.isFile()) return false;

        final DialogueConfig config;
        try {
            config = parseStartupDialogue(guideFile, filesDir);
        } catch (Exception error) {
            return false;
        }
        if (config == null || config.pages == null || config.pages.length == 0 ||
                config.pages.length > MAX_PAGES) {
            return false;
        }

        Bitmap background = decodeArgb(backgroundFile);
        if (background == null) return false;
        if (!nativeConfigure(STARTUP_DIALOGUE_ID, config.autoPlay, config.pages.length)) {
            background.recycle();
            return false;
        }
        if (!uploadBackground(background)) {
            background.recycle();
            nativeClear();
            return false;
        }
        background.recycle();

        for (int index = 0; index < config.pages.length; index++) {
            Bitmap page = renderDialogueText(config.pages[index]);
            if (page == null || !uploadPage(index, page)) {
                if (page != null) page.recycle();
                nativeClear();
                return false;
            }
            page.recycle();
        }

        if (!nativeCommit()) {
            nativeClear();
            return false;
        }
        return true;
    }

    private static DialogueConfig parseStartupDialogue(File plist, String filesDir) throws Exception {
        Document document = ImportedPlistXml.parse(plist, MAX_PLIST_BYTES);
        if (document == null) return null;
        Element root = firstElementChild(document.getDocumentElement(), "dict");
        if (root == null) return null;

        int dialogueCount = intValue(valueForKey(root, "dialogue_text_num"), 0);
        if (dialogueCount < STARTUP_DIALOGUE_ID) return null;
        boolean autoPlay = boolValue(valueForKey(root, "auto_play"), false);

        Element group = elementValue(valueForKey(root, STARTUP_DIALOGUE_KEY), "dict");
        if (group == null) return null;
        int pageCount = intValue(valueForKey(group, "text_num"), 0);
        if (pageCount <= 0 || pageCount > MAX_PAGES) return null;

        String[] pages = new String[pageCount];
        int languageColumn = languageColumn(Locale.getDefault());
        for (int pageIndex = 1; pageIndex <= pageCount; pageIndex++) {
            String key = String.format(Locale.US, "text_%02d", pageIndex);
            String localizationKey = stringValue(valueForKey(group, key));
            if (localizationKey == null || localizationKey.isEmpty()) return null;
            String localized = nativeLocalizedString(filesDir, localizationKey, languageColumn);
            if (localized == null || localized.isEmpty()) return null;
            pages[pageIndex - 1] = localized;
        }
        return new DialogueConfig(autoPlay, pages);
    }

    private static Element firstElementChild(Element parent, String tag) {
        if (parent == null) return null;
        for (Node node = parent.getFirstChild(); node != null; node = node.getNextSibling()) {
            if (node instanceof Element && tag.equals(node.getNodeName())) {
                return (Element) node;
            }
        }
        return null;
    }

    private static Node valueForKey(Element dictionary, String expectedKey) {
        if (dictionary == null || expectedKey == null) return null;
        for (Node node = dictionary.getFirstChild(); node != null; node = node.getNextSibling()) {
            if (!(node instanceof Element) || !"key".equals(node.getNodeName())) continue;
            String key = node.getTextContent();
            if (key == null || !expectedKey.equals(key.trim())) continue;
            for (Node value = node.getNextSibling(); value != null; value = value.getNextSibling()) {
                if (value instanceof Element) return value;
            }
            return null;
        }
        return null;
    }

    private static Element elementValue(Node value, String tag) {
        if (!(value instanceof Element) || !tag.equals(value.getNodeName())) return null;
        return (Element) value;
    }

    private static String stringValue(Node value) {
        if (!(value instanceof Element)) return null;
        String tag = value.getNodeName();
        if (!"string".equals(tag) && !"integer".equals(tag) && !"real".equals(tag)) {
            return null;
        }
        String text = value.getTextContent();
        return text == null ? null : text.trim();
    }

    private static int intValue(Node value, int fallback) {
        String text = stringValue(value);
        if (text == null || text.isEmpty()) return fallback;
        try {
            return Integer.parseInt(text);
        } catch (NumberFormatException error) {
            return fallback;
        }
    }

    private static boolean boolValue(Node value, boolean fallback) {
        if (!(value instanceof Element)) return fallback;
        if ("true".equals(value.getNodeName())) return true;
        if ("false".equals(value.getNodeName())) return false;
        String text = stringValue(value);
        if (text == null) return fallback;
        if ("1".equals(text) || "true".equalsIgnoreCase(text)) return true;
        if ("0".equals(text) || "false".equalsIgnoreCase(text)) return false;
        return fallback;
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

    private static Bitmap renderDialogueText(String text) {
        if (text == null || text.isEmpty()) return null;
        TextPaint paint = new TextPaint();
        paint.setColor(Color.WHITE);
        paint.setTextSize(TEXT_SIZE);
        paint.setAntiAlias(true);
        paint.setTypeface(Typeface.create("Arial", Typeface.NORMAL));

        StaticLayout layout = StaticLayout.Builder.obtain(text, 0, text.length(), paint, TEXT_WIDTH)
                .setAlignment(Layout.Alignment.ALIGN_NORMAL)
                .setIncludePad(false)
                .build();
        int height = layout.getHeight();
        if (height <= 0 || (long) TEXT_WIDTH * (long) height > MAX_PIXELS) return null;

        Bitmap bitmap = Bitmap.createBitmap(TEXT_WIDTH, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        layout.draw(canvas);
        return bitmap;
    }

    private static boolean uploadBackground(Bitmap bitmap) {
        int width = bitmap.getWidth();
        int height = bitmap.getHeight();
        int[] pixels = new int[width * height];
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
        return nativeUploadBackground(width, height, pixels);
    }

    private static boolean uploadPage(int pageIndex, Bitmap bitmap) {
        int width = bitmap.getWidth();
        int height = bitmap.getHeight();
        int[] pixels = new int[width * height];
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
        return nativeUploadPage(pageIndex, width, height, pixels);
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

    private static final class DialogueConfig {
        final boolean autoPlay;
        final String[] pages;

        DialogueConfig(boolean autoPlay, String[] pages) {
            this.autoPlay = autoPlay;
            this.pages = pages;
        }
    }
}
