package org.nevergone.recomp;

import android.content.ContentResolver;
import android.net.Uri;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.Comparator;
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
        deleteTree(staging);
        if (!staging.mkdirs() && !staging.isDirectory()) {
            throw new IOException("cannot create staging directory");
        }

        int importedFiles = 0;
        long importedBytes = 0;
        byte[] buffer = new byte[64 * 1024];
        Path stagingRoot = staging.toPath().toAbsolutePath().normalize();

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

                    Path output = stagingRoot.resolve(relative).normalize();
                    if (!output.startsWith(stagingRoot)) {
                        throw new IOException("unsafe APK entry: " + name);
                    }
                    Files.createDirectories(output.getParent());

                    long fileBytes = 0;
                    try (BufferedOutputStream out = new BufferedOutputStream(
                            new FileOutputStream(output.toFile()))) {
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

        File backup = new File(filesDir, "assets.previous");
        deleteTree(backup);
        if (target.exists()) {
            Files.move(target.toPath(), backup.toPath(), StandardCopyOption.REPLACE_EXISTING);
        }
        try {
            Files.move(staging.toPath(), target.toPath(), StandardCopyOption.REPLACE_EXISTING);
            deleteTree(backup);
        } catch (IOException error) {
            if (backup.exists() && !target.exists()) {
                Files.move(backup.toPath(), target.toPath(), StandardCopyOption.REPLACE_EXISTING);
            }
            throw error;
        }

        return new Result(importedFiles, importedBytes);
    }

    private static void deleteTree(File root) throws IOException {
        if (!root.exists()) {
            return;
        }
        try (var paths = Files.walk(root.toPath())) {
            paths.sorted(Comparator.reverseOrder()).forEach(path -> {
                try {
                    Files.deleteIfExists(path);
                } catch (IOException error) {
                    throw new DeleteFailure(error);
                }
            });
        } catch (DeleteFailure failure) {
            throw (IOException) failure.getCause();
        }
    }

    private static final class DeleteFailure extends RuntimeException {
        DeleteFailure(IOException cause) {
            super(cause);
        }
    }
}
