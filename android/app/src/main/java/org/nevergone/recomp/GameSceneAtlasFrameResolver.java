package org.nevergone.recomp;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

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
     * One immutable plist snapshot for a staging pass.
     *
     * The shipped runtime preloads sprite sheets into CCSpriteFrameCache before
     * spriteFrameByName lookups. Keeping the parsed DOMs here reproduces that
     * important loading shape without retaining them beyond the caller-owned
     * staging pass.
     */
    static final class Prepared {
        private final List<PreparedAtlas> atlases;

        Prepared(List<PreparedAtlas> atlases) {
            this.atlases = atlases;
        }

        Frame resolve(String requestedName) {
            if (!isSafeFrameName(requestedName)) return null;
            Frame resolved = null;
            for (PreparedAtlas prepared : atlases) {
                Match match = findFrame(prepared.frames, requestedName);
                if (match == null) continue;
                Frame candidate = parseFrame(
                        prepared.atlasIndex,
                        requestedName,
                        match,
                        prepared.atlas);
                if (candidate == null || resolved != null) {
                    // Fail closed on malformed matching metadata or duplicate
                    // membership, exactly as the one-shot resolver does.
                    return null;
                }
                resolved = candidate;
            }
            return resolved;
        }
    }

    private static final class PreparedAtlas {
        final int atlasIndex;
        final GameSceneAtlasResolver.Atlas atlas;
        final Element frames;

        PreparedAtlas(int atlasIndex, GameSceneAtlasResolver.Atlas atlas, Element frames) {
            this.atlasIndex = atlasIndex;
            this.atlas = atlas;
            this.frames = frames;
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
            ArrayList<PreparedAtlas> preparedAtlases = new ArrayList<>(atlases.size());

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
                Element frames = rootDict != null ? valueForKey(rootDict, "frames") : null;
                if (frames != null && !"dict".equals(frames.getTagName())) {
                    // The old one-shot lookup treated a non-dict frames value as
                    // no membership in this atlas rather than inventing a parse.
                    frames = null;
                }
                preparedAtlases.add(new PreparedAtlas(atlasIndex, atlas, frames));
            }
            return new Prepared(preparedAtlases);
        } catch (Exception error) {
            return null;
        }
    }

    private static Match findFrame(Element frames, String requestedName) {
        if (frames == null || !"dict".equals(frames.getTagName())) return null;

        Match found = null;
        for (Node node = frames.getFirstChild(); node != null; node = node.getNextSibling()) {
            if (!(node instanceof Element)) continue;
            Element key = (Element) node;
            if (!"key".equals(key.getTagName())) continue;
            String canonicalName = trimmedText(key);
            if (!isSafeFrameName(canonicalName)) return null;
            Element frame = nextElement(key);
            if (frame == null || !"dict".equals(frame.getTagName())) return null;

            boolean matches = requestedName.equals(canonicalName) || aliasMatches(frame, requestedName);
            if (!matches) continue;
            if (found != null) return null;
            found = new Match(canonicalName, frame);
        }
        return found;
    }

    private static Frame parseFrame(
            int atlasIndex,
            String requestedName,
            Match match,
            GameSceneAtlasResolver.Atlas atlas) {
        int[] textureRect = rectValue(valueForKey(match.frame, "textureRect"));
        int[] colorRect = rectValue(valueForKey(match.frame, "spriteColorRect"));
        int[] offset = pointValue(valueForKey(match.frame, "spriteOffset"));
        int[] spriteSize = pointValue(valueForKey(match.frame, "spriteSize"));
        int[] sourceSize = pointValue(valueForKey(match.frame, "spriteSourceSize"));
        Boolean rotated = booleanValue(valueForKey(match.frame, "textureRotated"));
        if (textureRect == null || colorRect == null || offset == null || spriteSize == null ||
                sourceSize == null || rotated == null || textureRect[0] < 0 || textureRect[1] < 0 ||
                textureRect[2] <= 0 || textureRect[3] <= 0 || colorRect[0] < 0 || colorRect[1] < 0 ||
                colorRect[2] <= 0 || colorRect[3] <= 0 || spriteSize[0] <= 0 || spriteSize[1] <= 0 ||
                sourceSize[0] <= 0 || sourceSize[1] <= 0 || colorRect[0] + colorRect[2] > sourceSize[0] ||
                colorRect[1] + colorRect[3] > sourceSize[1]) {
            return null;
        }
        return new Frame(
                atlasIndex, requestedName, match.canonicalName, atlas,
                textureRect, colorRect, offset, spriteSize, sourceSize, rotated.booleanValue());
    }

    private static boolean aliasMatches(Element frame, String requestedName) {
        Element aliases = valueForKey(frame, "aliases");
        if (aliases == null) return false;
        if (!"array".equals(aliases.getTagName())) return false;
        int count = 0;
        for (Node node = aliases.getFirstChild(); node != null; node = node.getNextSibling()) {
            if (!(node instanceof Element)) continue;
            if (++count > MAX_ALIASES) return false;
            Element item = (Element) node;
            if (!"string".equals(item.getTagName())) return false;
            String alias = trimmedText(item);
            if (!isSafeFrameName(alias)) return false;
            if (requestedName.equals(alias)) return true;
        }
        return false;
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
        try {
            return new int[]{Integer.parseInt(parts[0]), Integer.parseInt(parts[1])};
        } catch (NumberFormatException error) {
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
