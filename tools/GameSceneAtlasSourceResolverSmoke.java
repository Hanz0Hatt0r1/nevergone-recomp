package org.nevergone.recomp;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

public final class GameSceneAtlasSourceResolverSmoke {
    private static void write(File file, String content) throws Exception {
        File parent = file.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IllegalStateException("mkdir failed: " + parent);
        }
        Files.write(file.toPath(), content.getBytes(StandardCharsets.UTF_8));
    }

    private static String plist(String metadataValue) {
        String metadata = metadataValue == null
                ? ""
                : "<key>metadata</key><dict><key>textureFileName</key><string>" +
                        metadataValue + "</string></dict>";
        return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" +
                "<plist version=\"1.0\"><dict>" +
                "<key>frames</key><dict></dict>" + metadata +
                "</dict></plist>";
    }

    public static void main(String[] args) throws Exception {
        File root = Files.createTempDirectory("nevergone-atlas-source-smoke").toFile();
        File assetRoot = new File(root, "assets");
        File atlasDir = new File(assetRoot, "gamescene/l01/res");
        if (!atlasDir.mkdirs()) throw new IllegalStateException("mkdir failed");

        // Original metadata.textureFileName path: texture is resolved relative
        // to the resolved plist, not through a second search-path lookup.
        File metadataPlist = new File(atlasDir, "metadata.plist");
        File nestedTexture = new File(atlasDir, "textures/atlas-sheet.png");
        write(metadataPlist, plist("textures/atlas-sheet.png"));
        write(nestedTexture, "png-placeholder");
        GameSceneAtlasSourceResolver.Source source =
                GameSceneAtlasSourceResolver.resolveResolvedPlist(
                        assetRoot,
                        "l01/res/metadata.plist",
                        metadataPlist);
        assert source != null;
        assert source.textureNameFromMetadata;
        assert source.plistFile.equals(metadataPlist.getCanonicalFile());
        assert source.textureFile.equals(nestedTexture.getCanonicalFile());

        // If metadata.textureFileName is missing or empty, Cocos strips the
        // plist extension and appends .png beside the resolved plist.
        File fallbackPlist = new File(atlasDir, "fallback.plist");
        File fallbackTexture = new File(atlasDir, "fallback.png");
        write(fallbackPlist, plist(null));
        write(fallbackTexture, "png-placeholder");
        source = GameSceneAtlasSourceResolver.resolveResolvedPlist(
                assetRoot,
                "l01/res/fallback.plist",
                fallbackPlist);
        assert source != null;
        assert !source.textureNameFromMetadata;
        assert source.textureFile.equals(fallbackTexture.getCanonicalFile());

        File emptyMetadataPlist = new File(atlasDir, "empty.plist");
        File emptyFallbackTexture = new File(atlasDir, "empty.png");
        write(emptyMetadataPlist, plist(""));
        write(emptyFallbackTexture, "png-placeholder");
        source = GameSceneAtlasSourceResolver.resolveResolvedPlist(
                assetRoot,
                "l01/res/empty.plist",
                emptyMetadataPlist);
        assert source != null;
        assert !source.textureNameFromMetadata;
        assert source.textureFile.equals(emptyFallbackTexture.getCanonicalFile());

        // Relative metadata may normalize components, but imported resources
        // are never allowed to escape the user-owned asset root.
        File escapePlist = new File(atlasDir, "escape.plist");
        File outside = new File(root, "outside.png");
        write(outside, "outside");
        write(escapePlist, plist("../../../../outside.png"));
        assert GameSceneAtlasSourceResolver.resolveResolvedPlist(
                assetRoot,
                "l01/res/escape.plist",
                escapePlist) == null;

        // The public resource name itself remains a bounded safe relative path.
        assert GameSceneAtlasSourceResolver.resolveResolvedPlist(
                assetRoot,
                "../escape.plist",
                fallbackPlist) == null;
        assert GameSceneAtlasSourceResolver.resolveResolvedPlist(
                assetRoot,
                "/absolute.plist",
                fallbackPlist) == null;

        // Hardened XML parsing must not resolve external entities from imported
        // plists. A DOCTYPE-bearing input fails closed.
        File doctypePlist = new File(atlasDir, "doctype.plist");
        write(doctypePlist,
                "<?xml version=\"1.0\"?><!DOCTYPE plist [<!ENTITY xxe SYSTEM \"file:///etc/passwd\">]>" +
                "<plist><dict><key>metadata</key><dict><key>textureFileName</key>" +
                "<string>&xxe;</string></dict></dict></plist>");
        assert GameSceneAtlasSourceResolver.resolveResolvedPlist(
                assetRoot,
                "l01/res/doctype.plist",
                doctypePlist) == null;

        deleteTree(root);
    }

    private static void deleteTree(File file) {
        File[] children = file.listFiles();
        if (children != null) {
            for (File child : children) deleteTree(child);
        }
        if (!file.delete()) throw new IllegalStateException("delete failed: " + file);
    }
}
