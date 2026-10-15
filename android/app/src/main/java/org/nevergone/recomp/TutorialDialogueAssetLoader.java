package org.nevergone.recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.text.Layout;
import android.text.StaticLayout;
import android.text.TextPaint;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;

import java.io.File;
import java.util.Locale;

final class TutorialDialogueAssetLoader {
    private static final String GUIDE_PLIST = "Guide/guide_text.plist";
    private static final String DIALOGUE_DIR = "Dialogue_UI";
    private static final String BACKGROUND = "dialogue_bg.png";
    private static final long MAX_PLIST_BYTES = 2L * 1024L * 1024L;
    private static final long MAX_PIXELS = 16_777_216L;
    private static final int CONTENT_WIDTH = 450;

    private static native void nativeClear();
    private static native int nativeRequestedDialogueId();
    private static native boolean nativeBeginAssets(
            int dialogueId,
            boolean autoPlay,
            double fadeInSeconds,
            double fadeOutSeconds,
            int lineCount);
    private static native boolean nativeUploadBackground(
            int width, int height, int[] pixels);
    private static native boolean nativeUploadLine(
            int lineIndex,
            int npc,
            int npcPosition,
            double appearSeconds,
            double showDelaySeconds,
            boolean fadeIn,
            boolean fadeOut,
            int portraitWidth,
            int portraitHeight,
            int[] portraitPixels,
            int nameWidth,
            int nameHeight,
            int[] namePixels,
            int textWidth,
            int textHeight,
            int[] textPixels);
    private static native boolean nativeFinishAssets();
    private static native String nativeLocalizedString(
            String filesDir, String key, int languageColumn);
    private static native String nativeCurrentRoleName();
    private static native int nativeCurrentCareer();

    private TutorialDialogueAssetLoader() {}

    static boolean reloadFromFilesDir(String filesDir) {
        nativeClear();
        if (filesDir == null || filesDir.isEmpty()) return false;
        final int dialogueId = nativeRequestedDialogueId();
        // This loader intentionally implements only the first proven fresh-role
        // conversation. ID 9 belongs after LessonLayer::showDialogue().
        if (dialogueId != 8) return false;

        File assetRoot = new File(filesDir, "assets");
        File guideFile = new File(assetRoot, GUIDE_PLIST);
        File backgroundFile = new File(new File(assetRoot, DIALOGUE_DIR), BACKGROUND);
        if (!guideFile.isFile() || !backgroundFile.isFile()) return false;

        Bitmap background = null;
        try {
            Document document = ImportedPlistXml.parse(guideFile, MAX_PLIST_BYTES);
            Element root = document == null ? null : firstChild(document.getDocumentElement(), "dict");
            if (root == null) return false;

            Boolean autoPlay = boolValue(valueForKey(root, "auto_play"));
            Double fadeInSeconds = realValue(valueForKey(root, "fadein_time"));
            Double fadeOutSeconds = realValue(valueForKey(root, "fadeout_time"));
            Element dialogue = elementValue(valueForKey(root, "dialogue_text_08"), "dict");
            Integer lineCount = dialogue == null ? null : intValue(valueForKey(dialogue, "text_num"));
            if (autoPlay == null || !autoPlay || fadeInSeconds == null || fadeOutSeconds == null ||
                    lineCount == null || lineCount <= 0 || lineCount > 32) {
                return false;
            }

            background = decodeArgb(backgroundFile);
            if (background == null || !nativeBeginAssets(
                    dialogueId, true, fadeInSeconds, fadeOutSeconds, lineCount)) {
                return false;
            }
            if (!nativeUploadBackground(
                    background.getWidth(), background.getHeight(), pixels(background))) {
                return false;
            }

            final int languageColumn = languageColumn(Locale.getDefault());
            for (int zeroIndex = 0; zeroIndex < lineCount; zeroIndex++) {
                final int oneIndex = zeroIndex + 1;
                String suffix = String.format(Locale.US, "%02d", oneIndex);
                Integer npc = intValue(valueForKey(dialogue, "text_" + suffix + "_npc"));
                Integer npcPosition = intValue(valueForKey(dialogue, "npcPos" + suffix));
                Double appearSeconds = realValue(valueForKey(dialogue, "appearTime" + suffix));
                Double showDelaySeconds = realValue(valueForKey(dialogue, "showDelayTime" + suffix));
                Boolean fadeIn = boolValue(valueForKey(dialogue, "fadein_effect" + suffix));
                Boolean fadeOut = boolValue(valueForKey(dialogue, "fadeout_effect" + suffix));
                Integer textCount = intValue(valueForKey(dialogue, "text_" + suffix + "_count"));
                Element textMap = elementValue(valueForKey(dialogue, "text_" + suffix), "dict");
                String textKey = textMap == null ? null : stringValue(valueForKey(textMap, "1"));
                if (npc == null || npcPosition == null || appearSeconds == null ||
                        showDelaySeconds == null || fadeIn == null || fadeOut == null ||
                        textCount == null || textCount != 1 || textKey == null ||
                        (npcPosition != 0 && npcPosition != 1)) {
                    return false;
                }

                String text = nativeLocalizedString(filesDir, textKey, languageColumn);
                String name = speakerName(filesDir, npc, languageColumn);
                File portraitFile = portraitFile(assetRoot, npc);
                if (text == null || text.isEmpty() || name == null || name.isEmpty() ||
                        portraitFile == null || !portraitFile.isFile()) {
                    return false;
                }

                Bitmap portrait = null;
                Bitmap nameBitmap = null;
                Bitmap textBitmap = null;
                try {
                    portrait = decodeArgb(portraitFile);
                    nameBitmap = renderSingleLine(name, 23.0f);
                    textBitmap = renderWrapped(text, 20.0f, CONTENT_WIDTH);
                    if (portrait == null || nameBitmap == null || textBitmap == null) return false;
                    if (!nativeUploadLine(
                            zeroIndex,
                            npc,
                            npcPosition,
                            appearSeconds,
                            showDelaySeconds,
                            fadeIn,
                            fadeOut,
                            portrait.getWidth(),
                            portrait.getHeight(),
                            pixels(portrait),
                            nameBitmap.getWidth(),
                            nameBitmap.getHeight(),
                            pixels(nameBitmap),
                            textBitmap.getWidth(),
                            textBitmap.getHeight(),
                            pixels(textBitmap))) {
                        return false;
                    }
                } finally {
                    recycle(portrait);
                    recycle(nameBitmap);
                    recycle(textBitmap);
                }
            }

            if (!nativeFinishAssets()) return false;
            return true;
        } catch (Exception error) {
            return false;
        } finally {
            recycle(background);
        }
    }

    private static String speakerName(String filesDir, int npc, int languageColumn) {
        if (npc == 1) return nativeCurrentRoleName();
        if (npc == 7) return nativeLocalizedString(filesDir, "NPCName3", languageColumn);
        return null;
    }

    private static File portraitFile(File assetRoot, int npc) {
        File dialogueDir = new File(assetRoot, DIALOGUE_DIR);
        if (npc == 1) {
            int career = nativeCurrentCareer();
            if (career <= 0 || career > 99) return null;
            return new File(dialogueDir, String.format(Locale.US, "hero_%02d.png", career));
        }
        if (npc == 7) return new File(dialogueDir, "role_07.png");
        return null;
    }

    private static Bitmap decodeArgb(File file) {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        Bitmap decoded = BitmapFactory.decodeFile(file.getAbsolutePath(), options);
        if (decoded == null) return null;
        Bitmap result = decoded;
        if (decoded.getConfig() != Bitmap.Config.ARGB_8888) {
            result = decoded.copy(Bitmap.Config.ARGB_8888, false);
            decoded.recycle();
            if (result == null) return null;
        }
        long count = (long) result.getWidth() * (long) result.getHeight();
        if (result.getWidth() <= 0 || result.getHeight() <= 0 || count <= 0 || count > MAX_PIXELS) {
            result.recycle();
            return null;
        }
        return result;
    }

    private static Bitmap renderSingleLine(String text, float size) {
        if (text == null || text.isEmpty()) return null;
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setColor(Color.WHITE);
        paint.setTextSize(size);
        paint.setTypeface(Typeface.create("Arial", Typeface.NORMAL));
        Paint.FontMetricsInt metrics = paint.getFontMetricsInt();
        int width = Math.max(1, (int) Math.ceil(paint.measureText(text)));
        int height = Math.max(1, metrics.bottom - metrics.top);
        if ((long) width * height > MAX_PIXELS) return null;
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        canvas.drawText(text, 0.0f, -metrics.top, paint);
        return bitmap;
    }

    private static Bitmap renderWrapped(String text, float size, int width) {
        if (text == null || text.isEmpty() || width <= 0) return null;
        TextPaint paint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
        paint.setColor(Color.WHITE);
        paint.setTextSize(size);
        paint.setTypeface(Typeface.create("Arial", Typeface.NORMAL));
        StaticLayout layout = StaticLayout.Builder.obtain(text, 0, text.length(), paint, width)
                .setAlignment(Layout.Alignment.ALIGN_NORMAL)
                .setIncludePad(false)
                .build();
        int height = Math.max(1, layout.getHeight());
        if ((long) width * height > MAX_PIXELS) return null;
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        layout.draw(canvas);
        return bitmap;
    }

    private static int[] pixels(Bitmap bitmap) {
        int width = bitmap.getWidth();
        int height = bitmap.getHeight();
        int[] result = new int[width * height];
        bitmap.getPixels(result, 0, width, 0, 0, width, height);
        return result;
    }

    private static void recycle(Bitmap bitmap) {
        if (bitmap != null && !bitmap.isRecycled()) bitmap.recycle();
    }

    private static Element firstChild(Element parent, String tagName) {
        if (parent == null) return null;
        for (Node node = parent.getFirstChild(); node != null; node = node.getNextSibling()) {
            if (node instanceof Element && tagName.equals(node.getNodeName())) return (Element) node;
        }
        return null;
    }

    private static Node valueForKey(Element dict, String wantedKey) {
        if (dict == null || wantedKey == null) return null;
        for (Node node = dict.getFirstChild(); node != null; node = node.getNextSibling()) {
            if (!(node instanceof Element) || !"key".equals(node.getNodeName()) ||
                    !wantedKey.equals(node.getTextContent().trim())) {
                continue;
            }
            for (Node value = node.getNextSibling(); value != null; value = value.getNextSibling()) {
                if (value instanceof Element) return value;
            }
            return null;
        }
        return null;
    }

    private static Element elementValue(Node node, String tagName) {
        return node instanceof Element && tagName.equals(node.getNodeName()) ? (Element) node : null;
    }

    private static String stringValue(Node node) {
        if (!(node instanceof Element) || !"string".equals(node.getNodeName())) return null;
        return node.getTextContent();
    }

    private static Integer intValue(Node node) {
        if (!(node instanceof Element) || !"integer".equals(node.getNodeName())) return null;
        try {
            return Integer.parseInt(node.getTextContent().trim());
        } catch (NumberFormatException error) {
            return null;
        }
    }

    private static Double realValue(Node node) {
        if (!(node instanceof Element)) return null;
        String tag = node.getNodeName();
        if (!"real".equals(tag) && !"integer".equals(tag)) return null;
        try {
            double value = Double.parseDouble(node.getTextContent().trim());
            return Double.isFinite(value) && value >= 0.0 && value <= 120.0 ? value : null;
        } catch (NumberFormatException error) {
            return null;
        }
    }

    private static Boolean boolValue(Node node) {
        if (!(node instanceof Element)) return null;
        if ("true".equals(node.getNodeName())) return Boolean.TRUE;
        if ("false".equals(node.getNodeName())) return Boolean.FALSE;
        return null;
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
