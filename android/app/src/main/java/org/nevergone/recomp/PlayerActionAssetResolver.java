package org.nevergone.recomp;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;

import java.io.File;
import java.io.RandomAccessFile;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/** Resolves one evidence-backed pNN_actions.plist/actionNN entry in imported assets. */
final class PlayerActionAssetResolver {
    private static final long MAX_PLIST_BYTES = 8L * 1024L * 1024L;
    private static final long MAX_ACTIONS_DATA_BYTES = 64L * 1024L * 1024L;
    private static final int MAX_FILES = 30_000;
    private static final int MAX_DEPTH = 16;
    private static final int MAX_ACTION_NAME_LENGTH = 512;
    private static final int MAX_BUNDLE_ENTRIES = 4096;
    private static final int MAX_BUNDLE_LINE_BYTES = 512;

    static final class Result {
        final int career;
        final String actionKey;
        final String actionsPlistRelativePath;
        final String actionName;
        final String actionDataRelativePath;
        final String actionBundleRelativePath;
        final String actionBundleEntryName;

        Result(
                int career,
                String actionKey,
                String actionsPlistRelativePath,
                String actionName,
                String actionDataRelativePath,
                String actionBundleRelativePath,
                String actionBundleEntryName) {
            this.career = career;
            this.actionKey = actionKey;
            this.actionsPlistRelativePath = actionsPlistRelativePath;
            this.actionName = actionName;
            this.actionDataRelativePath = actionDataRelativePath;
            this.actionBundleRelativePath = actionBundleRelativePath;
            this.actionBundleEntryName = actionBundleEntryName;
        }

        boolean isEmbedded() {
            return actionBundleRelativePath != null && !actionBundleRelativePath.isEmpty() &&
                    actionBundleEntryName != null && !actionBundleEntryName.isEmpty();
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
            if (!validActionBasename(actionBasename)) return null;

            String plistRelative = relativePath(canonicalRoot, actionsPlist);
            if (plistRelative == null) return null;

            // The shipped SingleSelectHero path loads player_NN.actionsData
            // through ActionDataManager. Device evidence shows the action28
            // p01_i01.actData payload is an embedded member of that container,
            // not a sibling standalone file. Prefer the exact container when
            // present; retain standalone support for already extracted data.
            String bundleName = String.format(Locale.ROOT, "player_%02d.actionsData", career);
            List<File> bundleMatches = basenameMatches(files, bundleName);
            if (bundleMatches.size() > 1) return null;
            if (bundleMatches.size() == 1) {
                File bundle = bundleMatches.get(0);
                if (!bundleContainsUniqueEntry(bundle, actionBasename)) return null;
                String bundleRelative = relativePath(canonicalRoot, bundle);
                if (bundleRelative == null) return null;
                return new Result(
                        career,
                        actionKey,
                        plistRelative,
                        actionName,
                        null,
                        bundleRelative,
                        actionBasename);
            }

            File actionData = uniqueBasename(files, actionBasename);
            if (actionData == null || !actionData.isFile()) return null;
            String dataRelative = relativePath(canonicalRoot, actionData);
            if (dataRelative == null) return null;
            return new Result(
                    career,
                    actionKey,
                    plistRelative,
                    actionName,
                    dataRelative,
                    null,
                    null);
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

    private static List<File> basenameMatches(List<File> files, String wanted) {
        ArrayList<File> matches = new ArrayList<>();
        for (File file : files) {
            if (wanted.equalsIgnoreCase(file.getName())) matches.add(file);
        }
        return matches;
    }

    private static File uniqueBasename(List<File> files, String wanted) {
        List<File> matches = basenameMatches(files, wanted);
        return matches.size() == 1 ? matches.get(0) : null;
    }

    private static boolean bundleContainsUniqueEntry(File bundle, String wanted) {
        if (bundle == null || !bundle.isFile() || !validActionBasename(wanted) ||
                bundle.length() <= 0L || bundle.length() > MAX_ACTIONS_DATA_BYTES) {
            return false;
        }
        try (RandomAccessFile input = new RandomAccessFile(bundle, "r")) {
            long length = input.length();
            int entryCount = 0;
            int matchCount = 0;
            while (input.getFilePointer() < length) {
                if (++entryCount > MAX_BUNDLE_ENTRIES) return false;
                String name = readAsciiLine(input, length, MAX_BUNDLE_LINE_BYTES);
                String sizeText = readAsciiLine(input, length, 20);
                if (name == null || sizeText == null || !validActionBasename(name)) return false;
                long payloadSize = parseDecimalSize(sizeText);
                if (payloadSize < 0L) return false;
                long payloadStart = input.getFilePointer();
                if (payloadSize > length - payloadStart) return false;
                if (wanted.equals(name) && ++matchCount > 1) return false;
                input.seek(payloadStart + payloadSize);
                if (input.getFilePointer() < length) {
                    if (input.read() != '\n') return false;
                }
            }
            return matchCount == 1;
        } catch (Exception error) {
            return false;
        }
    }

    private static String readAsciiLine(RandomAccessFile input, long length, int maxBytes)
            throws Exception {
        StringBuilder out = new StringBuilder();
        while (input.getFilePointer() < length) {
            int value = input.read();
            if (value < 0) return null;
            if (value == '\n') return out.toString();
            if (value == '\r' || value == 0 || value > 0x7f || out.length() >= maxBytes) return null;
            out.append((char) value);
        }
        return null;
    }

    private static long parseDecimalSize(String text) {
        if (text == null || text.isEmpty()) return -1L;
        long value = 0L;
        for (int index = 0; index < text.length(); ++index) {
            char ch = text.charAt(index);
            if (ch < '0' || ch > '9') return -1L;
            int digit = ch - '0';
            if (value > (Long.MAX_VALUE - digit) / 10L) return -1L;
            value = value * 10L + digit;
        }
        return value;
    }

    private static String resolveActionName(File plist, String actionKey) throws Exception {
        Document document = ImportedPlistXml.parse(plist, MAX_PLIST_BYTES);
        if (document == null) return null;
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

    private static boolean validActionBasename(String value) {
        return validActionName(value) && value.indexOf('/') < 0 && value.indexOf('\\') < 0 &&
                value.indexOf('\n') < 0 && value.indexOf('\r') < 0 &&
                !".".equals(value) && !"..".equals(value);
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
