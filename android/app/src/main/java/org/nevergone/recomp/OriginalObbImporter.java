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
    private static final String REQUIRED_LEVEL = "gamescene/gs_list/pvp_scene.glData";
    private static final int MAX_FILES = 30_000;
    private static final long MAX_SINGLE_FILE_BYTES = 256L * 1024L * 1024L;
    private static final long MAX_TOTAL_BYTES = 2L * 1024L * 1024L * 1024L;

    static final class Result {
        final int files;
        final int decodedFiles;
        final long bytes;

        Result(int files, int decodedFiles, long bytes) {
            this.files = files;
            this.decodedFiles = decodedFiles;
            this.bytes = bytes;
        }
    }

    private OriginalObbImporter() {}

    static Result importAssets(ContentResolver resolver, Uri obbUri, File filesDir) throws IOException {
        File target = new File(filesDir, "assets");
        if (!target.isDirectory()) {
            throw new IOException("import the original APK before importing the OBB");
        }

        File staging = new File(filesDir, "assets.obb-importing");
        File backup = new File(filesDir, "assets.previous");
        deleteTree(staging);
        deleteTree(backup);
        if (!staging.mkdirs() && !staging.isDirectory()) {
            throw new IOException("cannot create OBB staging directory");
        }

        copyTree(target, staging);
        final String stagingRoot = staging.getCanonicalPath() + File.separator;
        int importedFiles = 0;
        int decodedFiles = 0;
        long importedBytes = 0;
        boolean foundAtlas = false;
        boolean foundLevel = false;
        byte[] buffer = new byte[64 * 1024];

        try (InputStream raw = resolver.openInputStream(obbUri)) {
            if (raw == null) throw new IOException("cannot open selected OBB");
            try (ZipInputStream zip = new ZipInputStream(new BufferedInputStream(raw))) {
                ZipEntry entry;
                while ((entry = zip.getNextEntry()) != null) {
                    String relative = mapAssetPath(entry.getName());
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
                    if (REQUIRED_LEVEL.equals(relative)) foundLevel = fileBytes > 0;
                    zip.closeEntry();
                }
            }
        } catch (IOException | RuntimeException error) {
            deleteTree(staging);
            throw error;
        }

        if (importedFiles == 0) {
            deleteTree(staging);
            throw new IOException("selected file does not contain an OBB assets tree");
        }
        if (!foundAtlas || !new File(staging, REQUIRED_ATLAS).isFile()) {
            deleteTree(staging);
            throw new IOException("selected OBB is missing Never Gone background resources");
        }
        if (!foundLevel || !new File(staging, REQUIRED_LEVEL).isFile()) {
            deleteTree(staging);
            throw new IOException("selected OBB is missing gamescene/gs_list/pvp_scene.glData");
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
        return new Result(importedFiles, decodedFiles, importedBytes);
    }

    private static String mapAssetPath(String name) {
        if (name == null) return null;
        while (name.startsWith("/")) name = name.substring(1);
        if (!name.startsWith(OBB_ASSET_ROOT)) return null;
        return name.substring(OBB_ASSET_ROOT.length());
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
