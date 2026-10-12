package org.nevergone.recomp;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;

import java.io.File;
import java.math.BigDecimal;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Resolves spriteFrameByName requests against the recovered ordered atlas list. */
final class GameSceneAtlasFrameResolver {
    private static final long MAX_PLIST_BYTES = 4L * 1024L * 1024L;
    private static final int MAX_FRAME_NAME_LENGTH = 512;
    private static final int MAX_ALIASES = 128;

    static final class Frame {
        final int atlasIndex;
        final String requestedName;
        final String canonicalName;
        final String plistRelativePath;
        final String textureRelativePath;
        final int textureX;
        final int textureY;
        final int textureWidth;
        final int textureHeight;
        final int colorX;
        final int colorY;
        final int colorWidth;
        final int colorHeight;
        final int offsetX;
        final int offsetY;
        final int spriteWidth;
        final int spriteHeight;
        final int sourceWidth;
        final int sourceHeight;
        final boolean rotated;

        Frame(
                int atlasIndex,
                String requestedName,
                String canonicalName,
                GameSceneAtlasResolver.Atlas atlas,
                int[] textureRect,
                int[] colorRect,
                int[] offset,
                int[] spriteSize,
                int[] sourceSize,
                boolean rotated) {
            this.atlasIndex = atlasIndex;
            this.requestedName = requestedName;
            this.canonicalName = canonicalName;
            this.plistRelativePath = atlas.plistRelativePath;
            this.textureRelativePath = atlas.textureRelativePath;
            this.textureX = textureRect[0];
            this.textureY = textureRect[1];
            this.textureWidth = textureRect[2];
            this.textureHeight = textureRect[3];
            this.colorX = colorRect[0];
            this.colorY = colorRect[1];
            this.colorWidth = colorRect[2];
            this.colorHeight = colorRect[3];
            this.offsetX = offset[0];
            this.offsetY = offset[1];
            this.spriteWidth = spriteSize[0];
            this.spriteHeight = spriteSize[1];
            this.sourceWidth = sourceSize[0];
            this.sourceHeight = sourceSize[1];
            this.rotated = rotated;
        }
    }

    /**
     * One immutable sprite-frame-cache snapshot for a staging pass.
     *
     * The shipped cocos2d-x 2.1.2 CCSpriteFrameCache preloads sheets in order.
     * Canonical duplicate frame names are skipped after the first load, while
     * aliases from accepted format-3 frames use the cache's latest mapping.
     */
    static final class Prepared {
        private final Map<String, PreparedFrame> framesByName;
        private final Map<String, String> aliases;

        Prepared(
                Map<String, PreparedFrame> framesByName,
                Map<String, String> aliases) {
            this.framesByName = framesByName;
            this.aliases = aliases;
        }

        Frame resolve(String requestedName) {
            if (!isSafeFrameName(requestedName)) return null;

            PreparedFrame prepared = framesByName.get(requestedName);
            String canonicalName = requestedName;
            if (prepared == null) {
                canonicalName = aliases.get(requestedName);
                if (canonicalName == null) return null;
                prepared = framesByName.get(canonicalName);
                if (prepared == null) return null;
            }

            Frame resolved = parseFrame(
                    prepared.atlasIndex,
                    requestedName,
                    new Match(canonicalName, prepared.frame),
                    prepared.atlas,
                    prepared.format);
            if (resolved != null) return resolved;

            // A null CCSpriteFrameCache lookup and a frame whose authoritative
            // metadata cannot be represented by the clean-room pixel path are
            // different states. The GameScene hidden fallback is valid only for
            // the former. Preserve membership here with a deliberately invalid
            // zero-sized frame so staging remains fail-closed on its existing
            // crop/reconstruction bounds instead of treating bad metadata as a
            // missing frame.
            return invalidMetadataFrame(
                    prepared.atlasIndex, requestedName, canonicalName, prepared.atlas);
        }
    }

    private static final class PreparedFrame {
        final int atlasIndex;
        final GameSceneAtlasResolver.Atlas atlas;
        final Element frame;
        final int format;

        PreparedFrame(
                int atlasIndex,
                GameSceneAtlasResolver.Atlas atlas,
                Element frame,
                int format) {
            this.atlasIndex = atlasIndex;
            this.atlas = atlas;
            this.frame = frame;
            this.format = format;
        }
    }

    private static final class Match {
        final String canonicalName;
        final Element frame;

        Match(String canonicalName, Element frame) {
            this.canonicalName = canonicalName;
            this.frame = frame;
        }
    }

    private GameSceneAtlasFrameResolver() {}

    static Frame resolve(
            File assetRoot,
            List<GameSceneAtlasResolver.Atlas> atlases,
            String requestedName) {
        Prepared prepared = prepare(assetRoot, atlases);
        return prepared != null ? prepared.resolve(requestedName) : null;
    }

    static Prepared prepare(
            File assetRoot,
            List<GameSceneAtlasResolver.Atlas> atlases) {
        if (assetRoot == null || atlases == null) return null;
        try {
            File canonicalRoot = assetRoot.getCanonicalFile();
            if (!canonicalRoot.isDirectory()) return null;
            String rootPrefix = canonicalRoot.getPath() + File.separator;
            LinkedHashMap<String, PreparedFrame> framesByName = new LinkedHashMap<>();
            HashMap<String, String> aliases = new HashMap<>();

            for (int atlasIndex = 0; atlasIndex < atlases.size(); ++atlasIndex) {
                GameSceneAtlasResolver.Atlas atlas = atlases.get(atlasIndex);
                if (atlas == null || atlas.plistRelativePath == null || atlas.textureRelativePath == null) {
                    return null;
                }
                File plist = new File(canonicalRoot, atlas.plistRelativePath).getCanonicalFile();
                File texture = new File(canonicalRoot, atlas.textureRelativePath).getCanonicalFile();
                if (!plist.getPath().startsWith(rootPrefix) || !texture.getPath().startsWith(rootPrefix) ||
                        !plist.isFile() || !texture.isFile() || plist.length() <= 0L ||
                        plist.length() > MAX_PLIST_BYTES) {
                    return null;
                }

                Document document = parseDocument(plist);
                Element plistRoot = document.getDocumentElement();
                Element rootDict = plistRoot != null ? firstChildElement(plistRoot, "dict") : null;
                if (rootDict == null) return null;
                Integer format = atlasFormat(rootDict);
                if (format == null) return null;
                Element frames = valueForKey(rootDict, "frames");
                if (frames == null) continue;
                if (!"dict".equals(frames.getTagName())) return null;
                if (!indexFrames(
                        atlasIndex, atlas, format.intValue(), frames, framesByName, aliases)) {
                    return null;
                }
            }
            return new Prepared(framesByName, aliases);
        } catch (Exception error) {
            return null;
        }
    }

    private static Integer atlasFormat(Element rootDict) {
        Element metadata = valueForKey(rootDict, "metadata");
        if (metadata == null) return Integer.valueOf(0);
        if (!"dict".equals(metadata.getTagName())) return null;
        Element formatValue = valueForKey(metadata, "format");
        if (formatValue == null) return Integer.valueOf(0);
        Integer format = integralScalar(formatValue);
        if (format == null || format.intValue() < 0 || format.intValue() > 3) return null;
        return format;
    }

    private static boolean indexFrames(
            int atlasIndex,
            GameSceneAtlasResolver.Atlas atlas,
            int format,
            Element frames,
            Map<String, PreparedFrame> framesByName,
            Map<String, String> aliases) {
        for (Node node = frames.getFirstChild(); node != null; node = node.getNextSibling()) {
            if (!(node instanceof Element)) continue;
            Element key = (Element) node;
            if (!"key".equals(key.getTagName())) continue;

            String canonicalName = trimmedText(key);
            if (!isSafeFrameName(canonicalName)) return false;
            Element frame = nextElement(key);
            if (frame == null || !"dict".equals(frame.getTagName())) return false;

            // cocos2d-x 2.1.2 skips a later duplicate canonical frame before
            // parsing any format-specific fields or aliases.
            if (framesByName.containsKey(canonicalName)) continue;
            framesByName.put(
                    canonicalName,
                    new PreparedFrame(atlasIndex, atlas, frame, format));

            // Aliases exist only in the format-3 branch of the original cache.
            if (format != 3) continue;
            Element aliasArray = valueForKey(frame, "aliases");
            if (aliasArray == null) continue;
            if (!"array".equals(aliasArray.getTagName())) return false;
            int aliasCount = 0;
            for (Node aliasNode = aliasArray.getFirstChild();
                    aliasNode != null;
                    aliasNode = aliasNode.getNextSibling()) {
                if (!(aliasNode instanceof Element)) continue;
                if (++aliasCount > MAX_ALIASES) return false;
                Element item = (Element) aliasNode;
                if (!"string".equals(item.getTagName())) return false;
                String alias = trimmedText(item);
                if (!isSafeFrameName(alias)) return false;
                aliases.put(alias, canonicalName);
            }
        }
        return true;
    }

    private static Frame parseFrame(
            int atlasIndex,
            String requestedName,
            Match match,
            GameSceneAtlasResolver.Atlas atlas,
            int format) {
        int[] textureRect;
        int[] offset;
        int[] spriteSize;
        int[] sourceSize;
        boolean rotated;

        if (format == 0) {
            Integer x = integralScalar(valueForKey(match.frame, "x"));
            Integer y = integralScalar(valueForKey(match.frame, "y"));
            Integer width = integralScalar(valueForKey(match.frame, "width"));
            Integer height = integralScalar(valueForKey(match.frame, "height"));
            Integer offsetX = integralScalar(valueForKey(match.frame, "offsetX"));
            Integer offsetY = integralScalar(valueForKey(match.frame, "offsetY"));
            Integer originalWidth = integralScalar(valueForKey(match.frame, "originalWidth"));
            Integer originalHeight = integralScalar(valueForKey(match.frame, "originalHeight"));
            if (x == null || y == null || width == null || height == null ||
                    offsetX == null || offsetY == null || originalWidth == null ||
                    originalHeight == null || originalWidth.intValue() == Integer.MIN_VALUE ||
                    originalHeight.intValue() == Integer.MIN_VALUE) {
                return null;
            }
            int sourceWidth = Math.abs(originalWidth.intValue());
            int sourceHeight = Math.abs(originalHeight.intValue());
            textureRect = new int[]{x.intValue(), y.intValue(), width.intValue(), height.intValue()};
            offset = new int[]{offsetX.intValue(), offsetY.intValue()};
            spriteSize = new int[]{width.intValue(), height.intValue()};
            sourceSize = new int[]{sourceWidth, sourceHeight};
            rotated = false;
        } else if (format == 1 || format == 2) {
            textureRect = rectValue(valueForKey(match.frame, "frame"));
            offset = pointValue(valueForKey(match.frame, "offset"));
            sourceSize = pointValue(valueForKey(match.frame, "sourceSize"));
            if (textureRect == null || offset == null || sourceSize == null) return null;
            spriteSize = new int[]{textureRect[2], textureRect[3]};
            if (format == 2) {
                Element rotatedValue = valueForKey(match.frame, "rotated");
                Boolean parsedRotated = rotatedValue == null
                        ? Boolean.FALSE
                        : booleanValue(rotatedValue);
                if (parsedRotated == null) return null;
                rotated = parsedRotated.booleanValue();
            } else {
                rotated = false;
            }
        } else if (format == 3) {
            textureRect = rectValue(valueForKey(match.frame, "textureRect"));
            offset = pointValue(valueForKey(match.frame, "spriteOffset"));
            spriteSize = pointValue(valueForKey(match.frame, "spriteSize"));
            sourceSize = pointValue(valueForKey(match.frame, "spriteSourceSize"));
            Element rotatedValue = valueForKey(match.frame, "textureRotated");
            Boolean parsedRotated = rotatedValue == null
                    ? Boolean.FALSE
                    : booleanValue(rotatedValue);
            if (textureRect == null || offset == null || spriteSize == null ||
                    sourceSize == null || parsedRotated == null) {
                return null;
            }
            rotated = parsedRotated.booleanValue();
        } else {
            return null;
        }

        return buildFrame(
                atlasIndex,
                requestedName,
                match.canonicalName,
                atlas,
                textureRect,
                offset,
                spriteSize,
                sourceSize,
                rotated);
    }

    private static Frame invalidMetadataFrame(
            int atlasIndex,
            String requestedName,
            String canonicalName,
            GameSceneAtlasResolver.Atlas atlas) {
        int[] zero = new int[]{0, 0};
        int[] zeroRect = new int[]{0, 0, 0, 0};
        return new Frame(
                atlasIndex,
                requestedName,
                canonicalName,
                atlas,
                zeroRect,
                zeroRect,
                zero,
                zero,
                zero,
                false);
    }

    private static Frame buildFrame(
            int atlasIndex,
            String requestedName,
            String canonicalName,
            GameSceneAtlasResolver.Atlas atlas,
            int[] textureRect,
            int[] offset,
            int[] spriteSize,
            int[] sourceSize,
            boolean rotated) {
        if (textureRect[0] < 0 || textureRect[1] < 0 ||
                spriteSize[0] <= 0 || spriteSize[1] <= 0 ||
                sourceSize[0] <= 0 || sourceSize[1] <= 0) {
            return null;
        }

        // CCSpriteFrame stores offset from the untrimmed sprite center to the
        // trimmed sprite center. Convert that common format-0..3 representation
        // into the project-owned top-left placement used during pixel staging.
        long colorX = ((long) sourceSize[0] - (long) spriteSize[0]) / 2L + offset[0];
        long colorY = ((long) sourceSize[1] - (long) spriteSize[1]) / 2L - offset[1];
        if (colorX < 0L || colorY < 0L ||
                colorX + spriteSize[0] > sourceSize[0] ||
                colorY + spriteSize[1] > sourceSize[1] ||
                colorX > Integer.MAX_VALUE || colorY > Integer.MAX_VALUE) {
            return null;
        }
        int[] colorRect = new int[]{
                (int) colorX, (int) colorY, spriteSize[0], spriteSize[1]};

        return new Frame(
                atlasIndex, requestedName, canonicalName, atlas,
                textureRect, colorRect, offset, spriteSize, sourceSize, rotated);
    }

    private static Document parseDocument(File plist) throws Exception {
        return ImportedPlistXml.parse(plist, MAX_PLIST_BYTES);
    }

    private static Element valueForKey(Element dict, String wantedKey) {
        if (dict == null) return null;
        for (Node node = dict.getFirstChild(); node != null; node = node.getNextSibling()) {
            if (!(node instanceof Element)) continue;
            Element key = (Element) node;
            if (!"key".equals(key.getTagName()) || !wantedKey.equals(trimmedText(key))) continue;
            return nextElement(key);
        }
        return null;
    }

    private static Element nextElement(Node node) {
        for (Node next = node.getNextSibling(); next != null; next = next.getNextSibling()) {
            if (next instanceof Element) return (Element) next;
        }
        return null;
    }

    private static Element firstChildElement(Element parent, String tagName) {
        for (Node node = parent.getFirstChild(); node != null; node = node.getNextSibling()) {
            if (node instanceof Element && tagName.equals(((Element) node).getTagName())) {
                return (Element) node;
            }
        }
        return null;
    }

    private static int[] rectValue(Element value) {
        if (value == null || !"string".equals(value.getTagName())) return null;
        String text = trimmedText(value).replaceAll("\\s+", "");
        if (!text.startsWith("{{") || !text.endsWith("}}")) return null;
        String inner = text.substring(2, text.length() - 2);
        String[] halves = inner.split("\\},\\{", -1);
        if (halves.length != 2) return null;
        int[] origin = parsePair(halves[0]);
        int[] size = parsePair(halves[1]);
        return origin != null && size != null
                ? new int[]{origin[0], origin[1], size[0], size[1]}
                : null;
    }

    private static int[] pointValue(Element value) {
        if (value == null || !"string".equals(value.getTagName())) return null;
        String text = trimmedText(value).replaceAll("\\s+", "");
        if (!text.startsWith("{") || !text.endsWith("}")) return null;
        return parsePair(text.substring(1, text.length() - 1));
    }

    private static int[] parsePair(String value) {
        String[] parts = value.split(",", -1);
        if (parts.length != 2) return null;
        Integer first = integralText(parts[0]);
        Integer second = integralText(parts[1]);
        return first != null && second != null
                ? new int[]{first.intValue(), second.intValue()}
                : null;
    }

    private static Integer integralScalar(Element value) {
        if (value == null) return null;
        String tag = value.getTagName();
        if (!"integer".equals(tag) && !"real".equals(tag) && !"string".equals(tag)) {
            return null;
        }
        return integralText(trimmedText(value));
    }

    private static Integer integralText(String text) {
        if (text == null) return null;
        try {
            return Integer.valueOf(new BigDecimal(text.trim()).intValueExact());
        } catch (NumberFormatException | ArithmeticException error) {
            // The original stores geometry as cocos2d float values. The current
            // project-owned pixel path has integer texel semantics, so accept
            // decimal spellings such as 12.0 only when they are exactly integral;
            // never round a genuinely fractional atlas coordinate.
            return null;
        }
    }

    private static Boolean booleanValue(Element value) {
        if (value == null) return null;
        if ("true".equals(value.getTagName())) return Boolean.TRUE;
        if ("false".equals(value.getTagName())) return Boolean.FALSE;
        if ("integer".equals(value.getTagName()) || "string".equals(value.getTagName())) {
            String text = trimmedText(value);
            if ("1".equals(text) || "true".equalsIgnoreCase(text)) return Boolean.TRUE;
            if ("0".equals(text) || "false".equalsIgnoreCase(text)) return Boolean.FALSE;
        }
        return null;
    }

    private static String trimmedText(Element value) {
        String text = value.getTextContent();
        return text != null ? text.trim() : "";
    }

    private static boolean isSafeFrameName(String value) {
        return value != null && !value.isEmpty() && value.length() <= MAX_FRAME_NAME_LENGTH &&
                value.indexOf('\0') < 0 && value.indexOf('/') < 0 && value.indexOf('\\') < 0 &&
                !".".equals(value) && !"..".equals(value);
    }
}
