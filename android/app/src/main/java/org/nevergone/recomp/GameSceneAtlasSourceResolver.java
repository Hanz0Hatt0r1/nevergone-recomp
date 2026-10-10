package org.nevergone.recomp;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.xml.sax.InputSource;

import java.io.File;
import java.io.FileInputStream;
import java.io.StringReader;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;

/** Resolves one recovered gsresfile04 plist to the texture file Cocos would load. */
final class GameSceneAtlasSourceResolver {
    private static final long MAX_PLIST_BYTES = 4L * 1024L * 1024L;
    private static final int MAX_TEXTURE_NAME_LENGTH = 1024;

    static final class Source {
        final String plistResourceName;
        final File plistFile;
        final File textureFile;
        final boolean textureNameFromMetadata;

        Source(
                String plistResourceName,
                File plistFile,
                File textureFile,
                boolean textureNameFromMetadata) {
            this.plistResourceName = plistResourceName;
            this.plistFile = plistFile;
            this.textureFile = textureFile;
            this.textureNameFromMetadata = textureNameFromMetadata;
        }
    }

    private GameSceneAtlasSourceResolver() {}

    private static native String nativeResolveResourcePath(String resourceName);

    static Source resolve(File assetRoot, String plistResourceName) {
        if (!isSafeResourceName(plistResourceName)) return null;
        String resolved = nativeResolveResourcePath(plistResourceName);
        if (resolved == null || resolved.isEmpty()) return null;
        return resolveResolvedPlist(assetRoot, plistResourceName, new File(resolved));
    }

    // Package-private so the exact metadata/fallback semantics can be tested on
    // the host without loading the Android JNI library.
    static Source resolveResolvedPlist(
            File assetRoot,
            String plistResourceName,
            File resolvedPlist) {
        if (assetRoot == null || resolvedPlist == null ||
                !isSafeResourceName(plistResourceName)) {
            return null;
        }

        try {
            File root = assetRoot.getCanonicalFile();
            File plist = resolvedPlist.getCanonicalFile();
            if (!root.isDirectory() || !plist.isFile() ||
                    plist.length() <= 0L || plist.length() > MAX_PLIST_BYTES ||
                    !containedBy(root, plist)) {
                return null;
            }

            String metadataTexture = readMetadataTextureFileName(plist);
            boolean fromMetadata = metadataTexture != null && !metadataTexture.isEmpty();
            File texture;
            if (fromMetadata) {
                if (!isSafeRelativeTextureName(metadataTexture)) return null;
                texture = new File(plist.getParentFile(), metadataTexture).getCanonicalFile();
            } else {
                String path = plist.getPath();
                int slash = Math.max(path.lastIndexOf('/'), path.lastIndexOf(File.separatorChar));
                int dot = path.lastIndexOf('.');
                if (dot <= slash) dot = path.length();
                texture = new File(path.substring(0, dot) + ".png").getCanonicalFile();
            }

            // The original fullPathFromRelativeFile() can normalize nested
            // relative components, but imported resources must still remain
            // inside the user-owned asset tree.
            if (!containedBy(root, texture) || !texture.isFile()) return null;
            return new Source(plistResourceName, plist, texture, fromMetadata);
        } catch (Exception error) {
            return null;
        }
    }

    private static String readMetadataTextureFileName(File plist) throws Exception {
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
        Element metadata = valueForKey(rootDict, "metadata");
        if (metadata == null || !"dict".equals(metadata.getTagName())) return null;
        Element textureName = valueForKey(metadata, "textureFileName");
        if (textureName == null || !"string".equals(textureName.getTagName())) return null;
        String value = textureName.getTextContent();
        return value != null ? value.trim() : null;
    }

    private static void trySetFeature(DocumentBuilderFactory factory, String name, boolean value) {
        try {
            factory.setFeature(name, value);
        } catch (Exception ignored) {
            // The entity resolver remains fail-closed for parsers that do not
            // expose one of the optional hardening feature flags.
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

    private static boolean containedBy(File root, File child) {
        String rootPath = root.getPath();
        String childPath = child.getPath();
        return childPath.equals(rootPath) || childPath.startsWith(rootPath + File.separator);
    }

    private static boolean isSafeResourceName(String value) {
        if (value == null || value.isEmpty() || value.length() > MAX_TEXTURE_NAME_LENGTH ||
                value.startsWith("/") || value.startsWith("\\") || value.indexOf('\0') >= 0) {
            return false;
        }
        String normalized = value.replace('\\', '/');
        for (String component : normalized.split("/")) {
            if (component.isEmpty() || ".".equals(component) || "..".equals(component)) return false;
        }
        return true;
    }

    private static boolean isSafeRelativeTextureName(String value) {
        return value != null && !value.isEmpty() && value.length() <= MAX_TEXTURE_NAME_LENGTH &&
                !new File(value).isAbsolute() && value.indexOf('\0') < 0;
    }
}
