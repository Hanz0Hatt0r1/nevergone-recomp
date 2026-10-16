package org.nevergone.recomp;

import android.content.ContentResolver;
import android.net.Uri;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

final class OriginalObbImporter {
    private static final String OBB_ASSET_ROOT = "assets/";
    private static final String REQUIRED_ATLAS =
            "gamescene_ui/LevelUI/Gate_Background_UI/Gate_BackgroundPNG_01.plist";
    private static final String REQUIRED_ATLAS_IMAGE =
            "gamescene_ui/LevelUI/Gate_Background_UI/Gate_BackgroundPNG_01.png";
    private static final String REQUIRED_LEVEL = "gamescene/gs_list/pvp_scene.glData";
    private static final int MAX_SCAN_FILES = 100_000;
    private static final int MAX_FILES = 100_000;
    private static final long MAX_SINGLE_FILE_BYTES = 256L * 1024L * 1024L;
    private static final long MAX_TOTAL_BYTES = 2L * 1024L * 1024L * 1024L;

    static final class Result {
        final int files;
        final int decodedFiles;
        final long bytes;
        final int scannedFiles;
        final String resourcePrefix;

        Result(int files, int decodedFiles, long bytes, int scannedFiles, String resourcePrefix) {
            this.files = files;
            this.decodedFiles = decodedFiles;
            this.bytes = bytes;
            this.scannedFiles = scannedFiles;
            this.resourcePrefix = resourcePrefix;
        }
    }

    private static final class ScanResult {
        final int files;
        final String resourcePrefix;

        ScanResult(int files, String resourcePrefix) {
            this.files = files;
            this.resourcePrefix = resourcePrefix;
        }
    }

    private OriginalObbImporter() {}

    static Result importAssets(ContentResolver resolver, Uri obbUri, File filesDir) throws IOException {
        File target = new File(filesDir, "assets");
        if (!target.isDirectory()) {
            throw new IOException("import the original APK before importing the OBB");
        }

        // OBB files seen in the wild are not guaranteed to expose the game tree as
        // an archive-root `assets/` directory. Some are rooted directly at the
        // resource tree while others wrap that tree in one or more directories.
        // Scan first and anchor the logical root using resources whose runtime paths
        // are already recovered. The second pass then imports the whole sibling tree.
        ScanResult scan = scanArchive(resolver, obbUri);
        if (scan.files == 0) {
            throw new IOException("selected file does not contain any OBB files");
        }

        File staging = new File(filesDir, "assets.obb-importing");
        File backup = new File(filesDir, "assets.previous");
        deleteTree(staging);
        deleteTree(backup);
        if (!staging.mkdirs() && !staging.isDirectory()) {
            throw new IOException("cannot create OBB staging directory");
        }

        try {
            copyTree(target, staging);
        } catch (IOException | RuntimeException error) {
            deleteTree(staging);
            throw error;
        }

        final String stagingRoot = staging.getCanonicalPath() + File.separator;
        int importedFiles = 0;
        int decodedFiles = 0;
        long importedBytes = 0;
        boolean foundAtlas = false;
        boolean foundAtlasImage = false;
        byte[] buffer = new byte[64 * 1024];

        try (InputStream raw = resolver.openInputStream(obbUri)) {
            if (raw == null) throw new IOException("cannot reopen selected OBB after scan");
            try (ZipInputStream zip = new ZipInputStream(new BufferedInputStream(raw))) {
                ZipEntry entry;
                while ((entry = zip.getNextEntry()) != null) {
                    String relative = mapAssetPath(entry.getName(), scan.resourcePrefix);
                    if (relative == null || relative.isEmpty() || entry.isDirectory()) {
                        zip.closeEntry();
                        continue;
                    }
                    if (++importedFiles > MAX_FILES) {
                        throw new IOException("OBB asset count exceeds safety limit");
                    }

                    File output = new File(staging, relative).getCanonicalFile();
                    if (!output.getPath().startsWith(stagingRoot)) {
                        throw new IOException("unsafe OBB entry: " + entry.getName());
                    }
                    File parent = output.getParentFile();
                    if (parent != null && !parent.mkdirs() && !parent.isDirectory()) {
                        throw new IOException("cannot create OBB asset directory: " + parent);
                    }

                    boolean decode = shouldDecode(relative);
                    if (decode) decodedFiles++;
                    long fileBytes = 0;
                    int decodeCounter = 0;
                    try (BufferedOutputStream out = new BufferedOutputStream(new FileOutputStream(output))) {
                        int read;
                        while ((read = zip.read(buffer)) != -1) {
                            fileBytes += read;
                            importedBytes += read;
                            if (fileBytes > MAX_SINGLE_FILE_BYTES) {
                                throw new IOException("OBB asset exceeds per-file safety limit: " + relative);
                            }
                            if (importedBytes > MAX_TOTAL_BYTES) {
                                throw new IOException("OBB import exceeds total safety limit");
                            }
                            if (decode) {
                                for (int index = 0; index < read; index++) {
                                    int value = buffer[index] & 0xff;
                                    buffer[index] = (byte) (((value ^ 1) - decodeCounter) & 0xff);
                                    decodeCounter++;
                                    if (decodeCounter == 0x7f) decodeCounter = 0;
                                }
                            }
                            out.write(buffer, 0, read);
                        }
                    }

                    if (REQUIRED_ATLAS.equals(relative)) foundAtlas = fileBytes > 0;
                    if (REQUIRED_ATLAS_IMAGE.equals(relative)) foundAtlasImage = fileBytes > 0;
                    zip.closeEntry();
                }
            }
        } catch (IOException | RuntimeException error) {
            deleteTree(staging);
            throw error;
        }

        if (importedFiles == 0) {
            deleteTree(staging);
            throw new IOException(
                    "OBB scan found files, but none mapped below the detected resource root: " +
                            printablePrefix(scan.resourcePrefix));
        }
        if (!foundAtlas || !foundAtlasImage ||
                !new File(staging, REQUIRED_ATLAS).isFile() ||
                !new File(staging, REQUIRED_ATLAS_IMAGE).isFile()) {
            deleteTree(staging);
            throw new IOException(
                    "selected OBB is missing Never Gone background resources after scanning root " +
                            printablePrefix(scan.resourcePrefix));
        }
        if (!new File(staging, REQUIRED_LEVEL).isFile()) {
            deleteTree(staging);
            throw new IOException("combined assets are missing gamescene/gs_list/pvp_scene.glData");
        }

        if (!target.renameTo(backup)) {
            deleteTree(staging);
            throw new IOException("cannot move current assets to backup");
        }
        if (!staging.renameTo(target)) {
            if (backup.exists() && !target.exists()) backup.renameTo(target);
            deleteTree(staging);
            throw new IOException("cannot activate imported OBB assets");
        }
        deleteTree(backup);
        return new Result(
                importedFiles,
                decodedFiles,
                importedBytes,
                scan.files,
                scan.resourcePrefix);
    }

    private static ScanResult scanArchive(ContentResolver resolver, Uri obbUri) throws IOException {
        int files = 0;
        String atlasPrefix = null;
        String atlasImagePrefix = null;
        String levelPrefix = null;
        String assetsPrefix = null;

        try (InputStream raw = resolver.openInputStream(obbUri)) {
            if (raw == null) throw new IOException("cannot open selected OBB");
            try (ZipInputStream zip = new ZipInputStream(new BufferedInputStream(raw))) {
                ZipEntry entry;
                while ((entry = zip.getNextEntry()) != null) {
                    if (!entry.isDirectory()) {
                        if (++files > MAX_SCAN_FILES) {
                            throw new IOException("OBB file count exceeds scan safety limit");
                        }
                        String name = normalizeEntryName(entry.getName());
                        if (name != null) {
                            String prefix = prefixBeforeSuffix(name, REQUIRED_ATLAS);
                            if (prefix != null) atlasPrefix = preferPrefix(atlasPrefix, prefix);

                            prefix = prefixBeforeSuffix(name, REQUIRED_ATLAS_IMAGE);
                            if (prefix != null) atlasImagePrefix = preferPrefix(atlasImagePrefix, prefix);

                            prefix = prefixBeforeSuffix(name, REQUIRED_LEVEL);
                            if (prefix != null) levelPrefix = preferPrefix(levelPrefix, prefix);

                            prefix = prefixThroughAssetsDirectory(name);
                            if (prefix != null) assetsPrefix = preferPrefix(assetsPrefix, prefix);
                        }
                    }
                    zip.closeEntry();
                }
            }
        }

        String resourcePrefix = chooseResourcePrefix(
                atlasPrefix,
                atlasImagePrefix,
                levelPrefix,
                assetsPrefix);
        return new ScanResult(files, resourcePrefix);
    }

    private static String chooseResourcePrefix(
            String atlasPrefix,
            String atlasImagePrefix,
            String levelPrefix,
            String assetsPrefix) throws IOException {
        if (atlasPrefix != null && atlasImagePrefix != null && !atlasPrefix.equals(atlasImagePrefix)) {
            throw new IOException(
                    "OBB atlas plist/png resolve to different resource roots: " +
                            printablePrefix(atlasPrefix) + " vs " + printablePrefix(atlasImagePrefix));
        }

        String anchored = atlasPrefix != null ? atlasPrefix : atlasImagePrefix;
        if (anchored != null) return anchored;
        if (levelPrefix != null) return levelPrefix;
        if (assetsPrefix != null) return assetsPrefix;

        // No known wrapper was detected. Treat the archive root itself as the game
        // resource root instead of discarding everything simply because `assets/`
        // is absent.
        return "";
    }

    private static String mapAssetPath(String name, String resourcePrefix) {
        String normalized = normalizeEntryName(name);
        if (normalized == null) return null;

        if (resourcePrefix != null && !resourcePrefix.isEmpty()) {
            if (!normalized.startsWith(resourcePrefix)) return null;
            normalized = normalized.substring(resourcePrefix.length());
        } else {
            String nestedAssetsPrefix = prefixThroughAssetsDirectory(normalized);
            if (nestedAssetsPrefix != null) {
                normalized = normalized.substring(nestedAssetsPrefix.length());
            }
        }

        while (normalized.startsWith("/")) normalized = normalized.substring(1);
        if (normalized.isEmpty() || hasUnsafeSegment(normalized)) return null;
        return normalized;
    }

    private static String normalizeEntryName(String name) {
        if (name == null) return null;
        String normalized = name.replace('\\', '/');
        while (normalized.startsWith("/")) normalized = normalized.substring(1);
        while (normalized.contains("//")) normalized = normalized.replace("//", "/");
        if (normalized.isEmpty() || hasUnsafeSegment(normalized)) return null;
        return normalized;
    }

    private static boolean hasUnsafeSegment(String path) {
        if (path.equals(".") || path.equals("..")) return true;
        String[] segments = path.split("/");
        for (String segment : segments) {
            if (segment.equals(".") || segment.equals("..")) return true;
        }
        return false;
    }

    private static String prefixBeforeSuffix(String name, String suffix) {
        if (!name.endsWith(suffix)) return null;
        return name.substring(0, name.length() - suffix.length());
    }

    private static String prefixThroughAssetsDirectory(String name) {
        if (name.startsWith(OBB_ASSET_ROOT)) return OBB_ASSET_ROOT;
        int index = name.indexOf("/" + OBB_ASSET_ROOT);
        if (index < 0) return null;
        return name.substring(0, index + 1 + OBB_ASSET_ROOT.length());
    }

    private static String preferPrefix(String current, String candidate) {
        if (candidate == null) return current;
        if (current == null) return candidate;
        // Prefer the shortest matching wrapper. It is the least likely to trim a
        // meaningful directory if a duplicated resource exists deeper in the OBB.
        return candidate.length() < current.length() ? candidate : current;
    }

    private static String printablePrefix(String prefix) {
        return prefix == null || prefix.isEmpty() ? "<archive-root>" : prefix;
    }

    private static boolean shouldDecode(String relative) {
        String lower = relative.toLowerCase(Locale.ROOT);
        return lower.endsWith(".lua") || lower.endsWith(".png") ||
                lower.endsWith(".hpc") || lower.endsWith(".csv");
    }

    private static void copyTree(File source, File destination) throws IOException {
        if (source.isDirectory()) {
            if (!destination.mkdirs() && !destination.isDirectory()) {
                throw new IOException("cannot create staging directory: " + destination);
            }
            File[] children = source.listFiles();
            if (children == null) throw new IOException("cannot list directory: " + source);
            for (File child : children) {
                copyTree(child, new File(destination, child.getName()));
            }
            return;
        }

        byte[] buffer = new byte[64 * 1024];
        try (BufferedInputStream in = new BufferedInputStream(new FileInputStream(source));
             BufferedOutputStream out = new BufferedOutputStream(new FileOutputStream(destination))) {
            int read;
            while ((read = in.read(buffer)) != -1) out.write(buffer, 0, read);
        }
    }

    private static void deleteTree(File root) throws IOException {
        if (!root.exists()) return;
        if (root.isDirectory()) {
            File[] children = root.listFiles();
            if (children == null) throw new IOException("cannot list directory: " + root);
            for (File child : children) deleteTree(child);
        }
        if (!root.delete() && root.exists()) {
            throw new IOException("cannot delete: " + root);
        }
    }
}
