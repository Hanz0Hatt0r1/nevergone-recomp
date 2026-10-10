package org.nevergone.recomp;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.xml.sax.InputSource;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.StringReader;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;

/**
 * Discovers the original GameScene scene-list plist without inventing its filename.
 *
 * The recovered 1.0.9 ARMv7 loader accepts a plist path, reads gs_num/gs%02d,
 * matches each entry's gamedatafile, and retains gsresfile04 as the array later
 * consumed by GameScene::loadingTex() through addSpriteFramesWithFile(). The
 * original call site that supplies the plist filename has no direct in-library
 * branch reference, so the clean-room runtime identifies the unique imported
 * plist by its proven schema and target gamedatafile instead.
 */
final class GameSceneAtlasListDiscovery {
    private static final int MAX_SCAN_DEPTH = 8;
    private static final int MAX_SCANNED_ENTRIES = 20_000;
    private static final long MAX_PLIST_BYTES = 4L * 1024L * 1024L;
    private static final int MAX_SCENE_ENTRIES = 4_096;
    private static final int MAX_ATLAS_PLISTS = 256;
    private static final int MAX_ATLAS_NAME_LENGTH = 512;

    static final class Result {
        final String sceneListRelativePath;
        final List<String> atlasPlists;

        Result(String sceneListRelativePath, List<String> atlasPlists) {
            this.sceneListRelativePath = sceneListRelativePath;
            this.atlasPlists = Collections.unmodifiableList(new ArrayList<>(atlasPlists));
        }
    }

    private static final class PendingDirectory {
        final File directory;
        final int depth;

        PendingDirectory(File directory, int depth) {
            this.directory = directory;
            this.depth = depth;
        }
    }

    private GameSceneAtlasListDiscovery() {}

    static Result discover(File assetRoot, String gameDataFile) {
        if (assetRoot == null || gameDataFile == null || gameDataFile.isEmpty()) return null;
        String targetBasename = basename(gameDataFile);
        if (!isSafeBasename(targetBasename)) return null;

        try {
            File canonicalAssetRoot = assetRoot.getCanonicalFile();
            if (!canonicalAssetRoot.isDirectory()) return null;
            File gameSceneRoot = new File(canonicalAssetRoot, "gamescene").getCanonicalFile();
            String assetPrefix = canonicalAssetRoot.getPath() + File.separator;
            if (!gameSceneRoot.isDirectory() || !gameSceneRoot.getPath().startsWith(assetPrefix)) {
                return null;
            }

            String gameScenePrefix = gameSceneRoot.getPath() + File.separator;
            ArrayDeque<PendingDirectory> pending = new ArrayDeque<>();
            pending.add(new PendingDirectory(gameSceneRoot, 0));
            int scannedEntries = 0;
            Result match = null;

            while (!pending.isEmpty()) {
                PendingDirectory current = pending.removeFirst();
                File[] children = current.directory.listFiles();
                if (children == null) continue;
                Arrays.sort(children, Comparator.comparing(File::getName));

                for (File child : children) {
                    if (++scannedEntries > MAX_SCANNED_ENTRIES) return null;
                    File canonicalChild = child.getCanonicalFile();
                    String childPath = canonicalChild.getPath();
                    if (!childPath.startsWith(gameScenePrefix)) continue;

                    if (canonicalChild.isDirectory()) {
                        if (current.depth < MAX_SCAN_DEPTH) {
                            pending.addLast(new PendingDirectory(canonicalChild, current.depth + 1));
                        }
                        continue;
                    }
                    if (!canonicalChild.isFile() ||
                            !canonicalChild.getName().toLowerCase(Locale.US).endsWith(".plist") ||
                            canonicalChild.length() <= 0L ||
                            canonicalChild.length() > MAX_PLIST_BYTES) {
                        continue;
                    }

                    List<String> atlases = readMatchingAtlasList(canonicalChild, targetBasename);
                    if (atlases == null) continue;
                    if (match != null) {
                        // More than one imported plist claims the same game-data file.
                        // Stay fail-closed rather than selecting by an invented priority.
                        return null;
                    }
                    String relative = canonicalAssetRoot.toURI().relativize(canonicalChild.toURI()).getPath();
                    if (relative.isEmpty() || relative.startsWith("/") || relative.contains("..")) {
                        return null;
                    }
                    match = new Result(relative, atlases);
                }
            }
            return match;
        } catch (Exception error) {
            return null;
        }
    }

    private static List<String> readMatchingAtlasList(File plist, String targetBasename) throws Exception {
        DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
        factory.setNamespaceAware(false);
        factory.setXIncludeAware(false);
        factory.setExpandEntityReferences(false);
        trySetFeature(factory, "http://apache.org/xml/features/disallow-doctype-decl", true);
        trySetFeature(factory, "http://xml.org/sax/features/external-general-entities", false);
        trySetFeature(factory, "http://xml.org/sax/features/external-parameter-entities", false);
        trySetFeature(factory, "http://apache.org/xml/features/nonvalidating/load-external-dtd", false);

        DocumentBuilder builder = factory.newDocumentBuilder();
        builder.setEntityResolver((publicId, systemId) -> new InputSource(new StringReader("")));
        Document document;
        try (FileInputStream input = new FileInputStream(plist)) {
            document = builder.parse(input);
        }

        Element plistRoot = document.getDocumentElement();
        if (plistRoot == null || !"plist".equals(plistRoot.getTagName())) return null;
        Element rootDict = firstChildElement(plistRoot, "dict");
        if (rootDict == null) return null;
        Element countValue = valueForKey(rootDict, "gs_num");
        Integer sceneCount = integerValue(countValue);
        if (sceneCount == null || sceneCount < 0 || sceneCount > MAX_SCENE_ENTRIES) return null;

        List<String> found = null;
        for (int index = 1; index <= sceneCount; ++index) {
            String entryKey = String.format(Locale.US, "gs%02d", index);
            Element entry = valueForKey(rootDict, entryKey);
            if (entry == null || !"dict".equals(entry.getTagName())) continue;
            String gameData = stringValue(valueForKey(entry, "gamedatafile"));
            if (gameData == null || !targetBasename.equals(basename(gameData))) continue;

            Element atlasValue = valueForKey(entry, "gsresfile04");
            List<String> atlases = stringArray(atlasValue);
            if (atlases == null || found != null) return null;
            found = atlases;
        }
        return found;
    }

    private static void trySetFeature(DocumentBuilderFactory factory, String name, boolean value) {
        try {
            factory.setFeature(name, value);
        } catch (Exception ignored) {
            // EntityResolver still prevents external resolution on parsers that
            // do not expose one of the optional hardening feature switches.
        }
    }

    private static Element valueForKey(Element dict, String wantedKey) {
        for (Node node = dict.getFirstChild(); node != null; node = node.getNextSibling()) {
            if (!(node instanceof Element)) continue;
            Element key = (Element) node;
            if (!"key".equals(key.getTagName()) || !wantedKey.equals(key.getTextContent())) continue;
            for (Node value = key.getNextSibling(); value != null; value = value.getNextSibling()) {
                if (value instanceof Element) return (Element) value;
            }
            return null;
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

    private static Integer integerValue(Element value) {
        if (value == null ||
                !("integer".equals(value.getTagName()) || "string".equals(value.getTagName()))) {
            return null;
        }
        try {
            return Integer.valueOf(value.getTextContent().trim());
        } catch (NumberFormatException ignored) {
            return null;
        }
    }

    private static String stringValue(Element value) {
        if (value == null || !"string".equals(value.getTagName())) return null;
        String text = value.getTextContent();
        return text != null ? text.trim() : null;
    }

    private static List<String> stringArray(Element value) {
        if (value == null || !"array".equals(value.getTagName())) return null;
        ArrayList<String> result = new ArrayList<>();
        for (Node node = value.getFirstChild(); node != null; node = node.getNextSibling()) {
            if (!(node instanceof Element)) continue;
            Element element = (Element) node;
            if (!"string".equals(element.getTagName()) || result.size() >= MAX_ATLAS_PLISTS) return null;
            String name = element.getTextContent();
            if (name == null) return null;
            name = name.trim();
            if (!isSafeRelativeResource(name)) return null;
            result.add(name);
        }
        return result;
    }

    private static boolean isSafeRelativeResource(String value) {
        if (value.isEmpty() || value.length() > MAX_ATLAS_NAME_LENGTH ||
                value.startsWith("/") || value.startsWith("\\") || value.indexOf('\0') >= 0) {
            return false;
        }
        String normalized = value.replace('\\', '/');
        for (String component : normalized.split("/")) {
            if (component.isEmpty() || ".".equals(component) || "..".equals(component)) return false;
        }
        return true;
    }

    private static boolean isSafeBasename(String value) {
        return value != null && !value.isEmpty() && !".".equals(value) && !"..".equals(value) &&
                value.indexOf('/') < 0 && value.indexOf('\\') < 0 && value.indexOf('\0') < 0;
    }

    private static String basename(String value) {
        if (value == null) return "";
        int slash = Math.max(value.lastIndexOf('/'), value.lastIndexOf('\\'));
        return slash >= 0 ? value.substring(slash + 1) : value;
    }
}
