package org.nevergone.recomp;

import android.content.ContentResolver;
import android.net.Uri;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

final class OriginalApkImporter {
    static final String APK_ASSET_PREFIX = "assets/assets/";
    private static final String REQUIRED_START_LUA = "Script/Game/StartLua.lua";
    private static final int MAX_FILES = 20_000;
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

    private OriginalApkImporter() {}

    static Result importAssets(ContentResolver resolver, Uri apkUri, File filesDir) throws IOException {
        File staging = new File(filesDir, "assets.importing");
        File target = new File(filesDir, "assets");
        File backup = new File(filesDir, "assets.previous");
        deleteTree(staging);
        deleteTree(backup);
        if (!staging.mkdirs() && !staging.isDirectory()) {
            throw new IOException("cannot create staging directory");
        }

        int importedFiles = 0;
        int decodedFiles = 0;
        long importedBytes = 0;
        byte[] buffer = new byte[64 * 1024];
        String stagingRoot = staging.getCanonicalPath() + File.separator;
        boolean foundStartLua = false;

        try (InputStream raw = resolver.openInputStream(apkUri)) {
            if (raw == null) {
                throw new IOException("cannot open selected APK");
            }
            try (ZipInputStream zip = new ZipInputStream(new BufferedInputStream(raw))) {
                ZipEntry entry;
                while ((entry = zip.getNextEntry()) != null) {
                    String name = entry.getName();
                    if (!name.startsWith(APK_ASSET_PREFIX) || entry.isDirectory()) {
                        zip.closeEntry();
                        continue;
                    }

                    String relative = name.substring(APK_ASSET_PREFIX.length());
                    if (relative.isEmpty()) {
                        zip.closeEntry();
                        continue;
                    }
                    if (++importedFiles > MAX_FILES) {
                        throw new IOException("asset count exceeds safety limit");
                    }

                    File output = new File(staging, relative).getCanonicalFile();
                    if (!output.getPath().startsWith(stagingRoot)) {
                        throw new IOException("unsafe APK entry: " + name);
                    }
                    File parent = output.getParentFile();
                    if (parent != null && !parent.mkdirs() && !parent.isDirectory()) {
                        throw new IOException("cannot create asset directory: " + parent);
                    }

                    boolean decode = shouldDecode(relative);
                    if (decode) {
                        decodedFiles++;
                    }

                    long fileBytes = 0;
                    int decodeCounter = 0;
                    try (BufferedOutputStream out = new BufferedOutputStream(
                            new FileOutputStream(output))) {
                        int read;
                        while ((read = zip.read(buffer)) != -1) {
                            fileBytes += read;
                            importedBytes += read;
                            if (fileBytes > MAX_SINGLE_FILE_BYTES) {
                                throw new IOException("asset exceeds per-file safety limit: " + relative);
                            }
                            if (importedBytes > MAX_TOTAL_BYTES) {
                                throw new IOException("asset import exceeds total safety limit");
                            }

                            if (decode) {
                                for (int index = 0; index < read; index++) {
                                    int value = buffer[index] & 0xff;
                                    buffer[index] = (byte) (((value ^ 1) - decodeCounter) & 0xff);
                                    decodeCounter++;
                                    if (decodeCounter == 0x7f) {
                                        decodeCounter = 0;
                                    }
                                }
                            }
                            out.write(buffer, 0, read);
                        }
                    }

                    if (REQUIRED_START_LUA.equals(relative)) {
                        foundStartLua = fileBytes > 0;
                    }
                    zip.closeEntry();
                }
            }
        } catch (IOException | RuntimeException error) {
            deleteTree(staging);
            throw error;
        }

        if (importedFiles == 0) {
            deleteTree(staging);
            throw new IOException("selected file does not contain Never Gone assets/assets tree");
        }
        if (!foundStartLua || !new File(staging, REQUIRED_START_LUA).isFile()) {
            deleteTree(staging);
            throw new IOException("selected APK is missing Script/Game/StartLua.lua");
        }
        validateDecodedStartLua(new File(staging, REQUIRED_START_LUA));

        if (target.exists() && !target.renameTo(backup)) {
            deleteTree(staging);
            throw new IOException("cannot move current assets to backup");
        }

        if (!staging.renameTo(target)) {
            if (backup.exists() && !target.exists()) {
                backup.renameTo(target);
            }
            deleteTree(staging);
            throw new IOException("cannot activate imported assets");
        }

        deleteTree(backup);
        return new Result(importedFiles, decodedFiles, importedBytes);
    }

    private static boolean shouldDecode(String relative) {
        String lower = relative.toLowerCase(Locale.ROOT);
        return lower.endsWith(".lua") || lower.endsWith(".png") ||
                lower.endsWith(".hpc") || lower.endsWith(".csv");
    }

    private static void validateDecodedStartLua(File startLua) throws IOException {
        try (InputStream input = new BufferedInputStream(new java.io.FileInputStream(startLua))) {
            int inspected = 0;
            int printable = 0;
            int value;
            while (inspected < 4096 && (value = input.read()) != -1) {
                inspected++;
                if (value == '\n' || value == '\r' || value == '\t' ||
                        (value >= 0x20 && value < 0x7f) || value >= 0x80) {
                    printable++;
                }
            }
            if (inspected == 0 || printable * 100 < inspected * 85) {
                throw new IOException("StartLua.lua did not decode to plausible Lua text");
            }
        }
    }

    private static void deleteTree(File root) throws IOException {
        if (!root.exists()) {
            return;
        }
        if (root.isDirectory()) {
            File[] children = root.listFiles();
            if (children == null) {
                throw new IOException("cannot list directory: " + root);
            }
            for (File child : children) {
                deleteTree(child);
            }
        }
        if (!root.delete() && root.exists()) {
            throw new IOException("cannot delete: " + root);
        }
    }
}
