package org.nevergone.recomp;

import android.content.ContentResolver;
import android.net.Uri;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

final class OriginalApkImporter {
    static final String APK_ASSET_PREFIX = "assets/assets/";

    static final class Result {
        final int files;
        final long bytes;

        Result(int files, long bytes) {
            this.files = files;
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
        long importedBytes = 0;
        byte[] buffer = new byte[64 * 1024];
        String stagingRoot = staging.getCanonicalPath() + File.separator;

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

                    File output = new File(staging, relative).getCanonicalFile();
                    if (!output.getPath().startsWith(stagingRoot)) {
                        throw new IOException("unsafe APK entry: " + name);
                    }
                    File parent = output.getParentFile();
                    if (parent != null && !parent.mkdirs() && !parent.isDirectory()) {
                        throw new IOException("cannot create asset directory: " + parent);
                    }

                    long fileBytes = 0;
                    try (BufferedOutputStream out = new BufferedOutputStream(
                            new FileOutputStream(output))) {
                        int read;
                        while ((read = zip.read(buffer)) != -1) {
                            out.write(buffer, 0, read);
                            fileBytes += read;
                        }
                    }
                    importedFiles++;
                    importedBytes += fileBytes;
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
        return new Result(importedFiles, importedBytes);
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
