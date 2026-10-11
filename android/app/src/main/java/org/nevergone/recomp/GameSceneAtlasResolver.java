package org.nevergone.recomp;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Locale;

/** Resolves recovered ordered atlas plist names and their texture images. */
final class GameSceneAtlasResolver {
    private static final String[] GAME_SCENE_SEARCH_ROOTS = {
            "gamescene",
            "gamescene/gs_actions/res",
            "gamescene/task",
            "gamescene/gs_list",
            "gamescene/scene_data_files",
            "gamescene/gs_res_image_file"
    };
    private static final long MAX_PLIST_BYTES = 4L * 1024L * 1024L;

    static final class Atlas {
        final String plistRelativePath;
        final String textureRelativePath;

        Atlas(String plistRelativePath, String textureRelativePath) {
            this.plistRelativePath = plistRelativePath;
            this.textureRelativePath = textureRelativePath;
        }
    }

    private GameSceneAtlasResolver() {}

    // MEPlayer::createActions preloads the selected career's default sprite
    // plists. Keep the candidate set bounded and let frame resolution reject
    // ambiguous names rather than guessing the player clothes selector.
    static List<Atlas> resolvePlayerDefaults(File assetRoot, int career) {
        if (assetRoot == null || career < 1 || career > 5) return null;
        try {
            File root = assetRoot.getCanonicalFile();
            String prefix = root.getPath() + File.separator;
            File directory = new File(root,
                    String.format(Locale.ROOT, "player/Player%02d_Res", career)).getCanonicalFile();
            if (!directory.getPath().startsWith(prefix) || !directory.isDirectory()) return null;
            File[] files = directory.listFiles();
            if (files == null || files.length > 512) return null;
            Arrays.sort(files, Comparator.comparing(File::getName));
            String namePrefix = String.format(Locale.ROOT, "p%02d_", career);
            ArrayList<Atlas> result = new ArrayList<>();
            for (File candidate : files) {
                File plist = candidate.getCanonicalFile();
                String name = plist.getName();
                if (!plist.getPath().startsWith(directory.getPath() + File.separator) ||
                        !plist.isFile() || !name.startsWith(namePrefix) ||
                        !name.endsWith("_default.plist")) continue;
                if (plist.length() <= 0L || plist.length() > MAX_PLIST_BYTES ||
                        result.size() >= 128) return null;
                File texture = resolveTextureForPlist(root, prefix, plist);
                if (texture == null || !texture.isFile()) return null;
                String plistRelative = relativePath(root, plist);
                String textureRelative = relativePath(root, texture);
                if (plistRelative == null || textureRelative == null) return null;
                result.add(new Atlas(plistRelative, textureRelative));
            }
            return result.isEmpty() ? null : Collections.unmodifiableList(result);
        } catch (Exception error) {
            return null;
        }
    }

    // Generic scene-list path retained for recovered GameLevelsData::gsresfile04
    // consumers outside the first-playable PVP route.
    static List<Atlas> resolve(File assetRoot, GameSceneAtlasListDiscovery.Result discovery) {
        if (discovery == null) return null;
        return resolve(assetRoot, discovery.atlasPlists);
    }

    // The first-playable PVP route gets this ordered list directly from the
    // binary GameLevels LoadGL_Global resource array (GameLevels + 0x24).
    static List<Atlas> resolve(File assetRoot, List<String> atlasNames) {
        if (assetRoot == null || atlasNames == null) return null;
        try {
            File canonicalRoot = assetRoot.getCanonicalFile();
            if (!canonicalRoot.isDirectory()) return null;
            String rootPrefix = canonicalRoot.getPath() + File.separator;
            ArrayList<Atlas> result = new ArrayList<>(atlasNames.size());
            for (String atlasName : atlasNames) {
                File plist = resolveUniquePlist(canonicalRoot, rootPrefix, atlasName);
                if (plist == null || plist.length() <= 0L || plist.length() > MAX_PLIST_BYTES) return null;
                File texture = resolveTextureForPlist(canonicalRoot, rootPrefix, plist);
                if (texture == null || !texture.isFile()) return null;
                String plistRelative = relativePath(canonicalRoot, plist);
                String textureRelative = relativePath(canonicalRoot, texture);
                if (plistRelative == null || textureRelative == null) return null;
                result.add(new Atlas(plistRelative, textureRelative));
            }
            return Collections.unmodifiableList(result);
        } catch (Exception error) {
            return null;
        }
    }

    private static File resolveUniquePlist(File root, String rootPrefix, String resourceName) throws Exception {
        if (!isSafeResource(resourceName)) return null;
        ArrayList<File> candidates = new ArrayList<>();
        addCandidate(root, rootPrefix, resourceName, candidates);
        for (String searchRoot : GAME_SCENE_SEARCH_ROOTS) {
            addCandidate(root, rootPrefix, searchRoot + "/" + resourceName, candidates);
        }
        File found = null;
        for (File candidate : candidates) {
            if (!candidate.isFile()) continue;
            if (found != null && !found.equals(candidate)) return null;
            found = candidate;
        }
        return found;
    }

    private static void addCandidate(
            File root,
            String rootPrefix,
            String relative,
            List<File> candidates) throws Exception {
        File candidate = new File(root, relative).getCanonicalFile();
        if (!candidate.getPath().startsWith(rootPrefix)) return;
        if (!candidates.contains(candidate)) candidates.add(candidate);
    }

    private static File resolveTextureForPlist(
            File root,
            String rootPrefix,
            File plist) throws Exception {
        String textureName = readDirectMetadataTextureFileName(plist);
        File candidate;
        if (textureName != null && !textureName.isEmpty()) {
            if (!isSafeResource(textureName)) return null;
            candidate = new File(plist.getParentFile(), textureName).getCanonicalFile();
        } else {
            String name = plist.getName();
            int dot = name.lastIndexOf('.');
            String pngName = (dot > 0 ? name.substring(0, dot) : name) + ".png";
            candidate = new File(plist.getParentFile(), pngName).getCanonicalFile();
        }
        return candidate.getPath().startsWith(rootPrefix) ? candidate : null;
    }

    private static String readDirectMetadataTextureFileName(File plist) throws Exception {
        Document document = ImportedPlistXml.parse(plist, MAX_PLIST_BYTES);
        if (document == null) return null;
        Element plistRoot = document.getDocumentElement();
        Element rootDict = plistRoot != null ? firstChildElement(plistRoot, "dict") : null;
        Element metadata = rootDict != null ? valueForKey(rootDict, "metadata") : null;
        if (metadata == null || !"dict".equals(metadata.getTagName())) return null;
        Element texture = valueForKey(metadata, "textureFileName");
        if (texture == null || !"string".equals(texture.getTagName())) return null;
        String value = texture.getTextContent();
        return value != null ? value.trim() : null;
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

    private static boolean isSafeResource(String value) {
        if (value == null || value.isEmpty() || value.startsWith("/") || value.startsWith("\\") ||
                value.indexOf('\0') >= 0) return false;
        String normalized = value.replace('\\', '/');
        for (String component : normalized.split("/")) {
            if (component.isEmpty() || ".".equals(component) || "..".equals(component)) return false;
        }
        return true;
    }

    private static String relativePath(File root, File file) {
        String value = root.toURI().relativize(file.toURI()).getPath();
        if (value.isEmpty() || value.startsWith("/") || value.contains("..")) return null;
        return value;
    }
}
