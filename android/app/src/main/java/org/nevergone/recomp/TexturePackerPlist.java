package org.nevergone.recomp;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserFactory;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

final class TexturePackerPlist {
    static final class Frame {
        final int textureX;
        final int textureY;
        final int textureWidth;
        final int textureHeight;
        final int sourceWidth;
        final int sourceHeight;
        final int offsetX;
        final int offsetY;
        final boolean rotated;

        Frame(
                int textureX,
                int textureY,
                int textureWidth,
                int textureHeight,
                int sourceWidth,
                int sourceHeight,
                int offsetX,
                int offsetY,
                boolean rotated) {
            this.textureX = textureX;
            this.textureY = textureY;
            this.textureWidth = textureWidth;
            this.textureHeight = textureHeight;
            this.sourceWidth = sourceWidth;
            this.sourceHeight = sourceHeight;
            this.offsetX = offsetX;
            this.offsetY = offsetY;
            this.rotated = rotated;
        }
    }

    private TexturePackerPlist() {}

    static Frame readFrame(File plistFile, String frameName) throws Exception {
        XmlPullParserFactory factory = XmlPullParserFactory.newInstance();
        factory.setNamespaceAware(false);
        XmlPullParser parser = factory.newPullParser();
        try (InputStream input = new FileInputStream(plistFile)) {
            parser.setInput(input, "UTF-8");
            int event;
            while ((event = parser.next()) != XmlPullParser.END_DOCUMENT) {
                if (event != XmlPullParser.START_TAG || !"key".equals(parser.getName())) {
                    continue;
                }
                String key = parser.nextText();
                if (!frameName.equals(key)) {
                    continue;
                }
                moveToStartTag(parser, "dict");
                return readFrameDict(parser);
            }
        }
        return null;
    }

    private static Frame readFrameDict(XmlPullParser parser) throws Exception {
        String textureRect = null;
        String sourceSize = null;
        String offset = null;
        boolean rotated = false;
        int depth = parser.getDepth();

        while (true) {
            int event = parser.next();
            if (event == XmlPullParser.END_DOCUMENT) {
                break;
            }
            if (event == XmlPullParser.END_TAG && parser.getDepth() == depth && "dict".equals(parser.getName())) {
                break;
            }
            if (event != XmlPullParser.START_TAG || !"key".equals(parser.getName())) {
                continue;
            }

            String key = parser.nextText();
            int valueEvent = parser.next();
            while (valueEvent != XmlPullParser.START_TAG && valueEvent != XmlPullParser.END_DOCUMENT) {
                valueEvent = parser.next();
            }
            if (valueEvent == XmlPullParser.END_DOCUMENT) {
                break;
            }

            String valueTag = parser.getName();
            if ("textureRect".equals(key) && "string".equals(valueTag)) {
                textureRect = parser.nextText();
            } else if ("spriteSourceSize".equals(key) && "string".equals(valueTag)) {
                sourceSize = parser.nextText();
            } else if ("spriteOffset".equals(key) && "string".equals(valueTag)) {
                offset = parser.nextText();
            } else if ("textureRotated".equals(key)) {
                rotated = "true".equals(valueTag);
            } else {
                skipValue(parser);
            }
        }

        int[] rect = parseInts(textureRect, 4);
        int[] source = parseInts(sourceSize, 2);
        int[] spriteOffset = parseInts(offset, 2);
        if (rect == null || source == null || spriteOffset == null) {
            return null;
        }
        return new Frame(
                rect[0], rect[1], rect[2], rect[3],
                source[0], source[1],
                spriteOffset[0], spriteOffset[1],
                rotated);
    }

    private static void moveToStartTag(XmlPullParser parser, String name) throws Exception {
        while (true) {
            int event = parser.next();
            if (event == XmlPullParser.END_DOCUMENT) {
                throw new IllegalArgumentException("Unexpected end of plist");
            }
            if (event == XmlPullParser.START_TAG && name.equals(parser.getName())) {
                return;
            }
        }
    }

    private static void skipValue(XmlPullParser parser) throws Exception {
        if (parser.getEventType() != XmlPullParser.START_TAG) {
            return;
        }
        String tag = parser.getName();
        if ("true".equals(tag) || "false".equals(tag)) {
            return;
        }
        int depth = parser.getDepth();
        while (true) {
            int event = parser.next();
            if (event == XmlPullParser.END_DOCUMENT) {
                return;
            }
            if (event == XmlPullParser.END_TAG && parser.getDepth() == depth) {
                return;
            }
        }
    }

    private static int[] parseInts(String value, int expected) {
        if (value == null) {
            return null;
        }
        List<Integer> values = new ArrayList<>();
        int index = 0;
        while (index < value.length()) {
            while (index < value.length() && value.charAt(index) != '-' && !Character.isDigit(value.charAt(index))) {
                index++;
            }
            if (index >= value.length()) {
                break;
            }
            int start = index++;
            while (index < value.length() && Character.isDigit(value.charAt(index))) {
                index++;
            }
            try {
                values.add(Integer.parseInt(value.substring(start, index)));
            } catch (NumberFormatException ignored) {
                return null;
            }
        }
        if (values.size() < expected) {
            return null;
        }
        int[] result = new int[expected];
        for (int i = 0; i < expected; i++) {
            result[i] = values.get(i);
        }
        return result;
    }
}
