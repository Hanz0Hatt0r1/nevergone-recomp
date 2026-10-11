package org.nevergone.recomp;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.xml.sax.InputSource;

import java.io.File;
import java.io.FileInputStream;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;

/** Resolves one evidence-backed pNN_actions.plist/actionNN entry in imported assets. */
final class PlayerActionAssetResolver {
    private static final long MAX_PLIST_BYTES = 8L * 1024L * 1024L;
    private static final int MAX_FILES = 30_000;
    private static final int MAX_DEPTH = 16;
    private static final int MAX_ACTION_NAME_LENGTH = 512;

    static final class Result {
        final int career;
        final String actionKey;
        final String actionsPlistRelativePath;
        final String actionName;
        final String actionDataRelativePath;

        Result(
                int career,
                String actionKey,
                String actionsPlistRelativePath,
                String actionName,
                String actionDataRelativePath) {
            this.career = career;
            this.actionKey = actionKey;
            this.actionsPlistRelativePath = actionsPlistRelativePath;
            this.actionName = actionName;
            this.actionDataRelativePath = actionDataRelativePath;
        }
    }

    private PlayerActionAssetResolver() {}

    static Result resolveInitial(File assetRoot, int career) {
        return resolve(assetRoot, career, "action28");
    }

    static Result resolve(File assetRoot, int career, String actionKey) {
        if (assetRoot == null || career < 1 || career > 5 || !validActionKey(actionKey)) return null;
        try {
            File canonicalRoot = assetRoot.getCanonicalFile();
            if (!canonicalRoot.isDirectory()) return null;
            List<File> files = new ArrayList<>();
            if (!collectFiles(canonicalRoot, canonicalRoot, 0, files)) return null;

            String plistName = String.format(Locale.ROOT, "p%02d_actions.plist", career);
            File actionsPlist = uniqueBasename(files, plistName);
            if (actionsPlist == null || actionsPlist.length() <= 0L ||
                    actionsPlist.length() > MAX_PLIST_BYTES) {
                return null;
            }

            String actionName = resolveActionName(actionsPlist, actionKey);
            if (!validActionName(actionName)) return null;
            String actionBasename = new File(actionName.replace('\\', '/')).getName();
            if (!validActionName(actionBasename)) return null;

            File actionData = uniqueBasename(files, actionBasename);
            if (actionData == null || !actionData.isFile()) return null;

            String plistRelative = relativePath(canonicalRoot, actionsPlist);
            String dataRelative = relativePath(canonicalRoot, actionData);
            if (plistRelative == null || dataRelative == null) return null;
            return new Result(career, actionKey, plistRelative, actionName, dataRelative);
        } catch (Exception error) {
            return null;
        }
    }

    private static boolean collectFiles(
            File root,
            File directory,
            int depth,
            List<File> files) throws Exception {
        if (depth > MAX_DEPTH || directory == null || !directory.isDirectory()) return false;
        File[] children = directory.listFiles();
        if (children == null) return false;
        for (File child : children) {
            File canonical = child.getCanonicalFile();
            if (!withinRoot(root, canonical)) return false;
            if (canonical.isFile()) {
                if (files.size() >= MAX_FILES) return false;
                files.add(canonical);
            }
        }
        for (File child : children) {
            File canonical = child.getCanonicalFile();
            if (canonical.isDirectory() && !collectFiles(root, canonical, depth + 1, files)) {
                return false;
            }
        }
        return true;
    }

    private static File uniqueBasename(List<File> files, String wanted) {
        File found = null;
        for (File file : files) {
            if (!wanted.equalsIgnoreCase(file.getName())) continue;
            if (found != null) return null;
            found = file;
        }
        return found;
    }

    private static String resolveActionName(File plist, String actionKey) throws Exception {
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
        Element rootDict = plistRoot != null ? firstChildElement(plistRoot, "dict") : null;
        Element action = valueForKey(rootDict, actionKey);
        if (action == null || !"dict".equals(action.getTagName())) return null;
        Element name = valueForKey(action, "name");
        if (name == null || !"string".equals(name.getTagName())) return null;
        String value = name.getTextContent();
        return value != null ? value.trim() : null;
    }

    private static Element valueForKey(Element dict, String wantedKey) {
        if (dict == null) return null;
        for (Node node = dict.getFirstChild(); node != null; node = node.getNextSibling()) {
            if (!(node instanceof Element)) continue;
            Element key = (Element) node;
            if (!"key".equals(key.getTagName())) continue;
            String text = key.getTextContent();
            if (text == null || !wantedKey.equals(text.trim())) continue;
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

    private static void trySetFeature(DocumentBuilderFactory factory, String name, boolean value) {
        try {
            factory.setFeature(name, value);
        } catch (Exception ignored) {
        }
    }

    private static boolean validActionKey(String value) {
        if (value == null || !value.startsWith("action") || value.length() <= 6) return false;
        for (int index = 6; index < value.length(); ++index) {
            if (!Character.isDigit(value.charAt(index))) return false;
        }
        try {
            return Integer.parseInt(value.substring(6)) > 0;
        } catch (NumberFormatException error) {
            return false;
        }
    }

    private static boolean validActionName(String value) {
        return value != null && !value.isEmpty() && value.length() <= MAX_ACTION_NAME_LENGTH &&
                value.indexOf('\0') < 0;
    }

    private static String relativePath(File root, File file) throws Exception {
        File canonicalRoot = root.getCanonicalFile();
        File canonicalFile = file.getCanonicalFile();
        if (!withinRoot(canonicalRoot, canonicalFile)) return null;
        String rootPath = canonicalRoot.getPath();
        String filePath = canonicalFile.getPath();
        if (filePath.equals(rootPath)) return null;
        String relative = filePath.substring(rootPath.length() + 1);
        return relative.replace(File.separatorChar, '/');
    }

    private static boolean withinRoot(File root, File file) {
        String rootPath = root.getPath();
        String filePath = file.getPath();
        return filePath.equals(rootPath) || filePath.startsWith(rootPath + File.separator);
    }
}
