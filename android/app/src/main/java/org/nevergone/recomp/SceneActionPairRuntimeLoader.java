package org.nevergone.recomp;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/** Stages one evidence-backed klhuo SceneActions born/idle document pair. */
final class SceneActionPairRuntimeLoader {
    private static final int MAX_FILES = 30_000;
    private static final int MAX_DEPTH = 16;
    static final int SLOT_BORN = 0;
    static final int SLOT_IDLE = 1;

    private static int loadedVariant;
    private static String bornRelativePath;
    private static String idleRelativePath;

    private SceneActionPairRuntimeLoader() {}

    private static native boolean nativeLoadSlot(int variant, int slot, String relativePath);
    private static native boolean nativePairReady(int variant);
    private static native String nativeInitialAtlasPlist(int slot);
    private static native String nativeInitialFrameName(int slot);
    private static native int nativeInitialMode(int slot);
    private static native String nativeCurrentAtlasPlist(int slot, int frameIndex);
    private static native String nativeCurrentFrameName(int slot, int frameIndex);
    private static native int nativeCurrentMode(int slot, int frameIndex);
    private static native void nativeReset();
    private static native String nativeStatusReport();

    static void invalidate() {
        loadedVariant = 0;
        bornRelativePath = null;
        idleRelativePath = null;
        nativeReset();
    }

    // The caller must provide the recovered variant. This loader deliberately
    // does not derive a variant from file availability or choose variant 01 as
    // a fallback because the original consumes the process-global lrand48 stream.
    static boolean stageVariant(File assetRoot, int variant) {
        if (assetRoot == null || variant < 1 || variant > 4) return false;
        if (loadedVariant == variant && bornRelativePath != null && idleRelativePath != null &&
                nativePairReady(variant)) {
            return true;
        }

        Pair pair = resolve(assetRoot, variant);
        if (pair == null) return false;
        if (loadedVariant != 0 && loadedVariant != variant) invalidate();

        if (!nativeLoadSlot(variant, SLOT_BORN, pair.bornRelativePath)) return false;
        if (!nativeLoadSlot(variant, SLOT_IDLE, pair.idleRelativePath)) return false;
        if (!nativePairReady(variant)) return false;

        loadedVariant = variant;
        bornRelativePath = pair.bornRelativePath;
        idleRelativePath = pair.idleRelativePath;
        return true;
    }

    static InitialFrame initialFrame(int slot) {
        if (slot != SLOT_BORN && slot != SLOT_IDLE) return null;
        String atlas = nativeInitialAtlasPlist(slot);
        String frame = nativeInitialFrameName(slot);
        int mode = nativeInitialMode(slot);
        if (atlas == null || atlas.isEmpty() || frame == null || frame.isEmpty() || mode < 0) {
            return null;
        }
        return new InitialFrame(atlas, frame, mode);
    }

    static CurrentFrame currentFrame(int slot, int frameIndex) {
        if ((slot != SLOT_BORN && slot != SLOT_IDLE) || frameIndex < 0) return null;
        String atlas = nativeCurrentAtlasPlist(slot, frameIndex);
        String frame = nativeCurrentFrameName(slot, frameIndex);
        int mode = nativeCurrentMode(slot, frameIndex);
        if (atlas == null || atlas.isEmpty() || frame == null || frame.isEmpty() || mode < 0) {
            return null;
        }
        return new CurrentFrame(atlas, frame, mode, frameIndex);
    }

    static String statusReport() {
        StringBuilder out = new StringBuilder();
        String nativeReport = nativeStatusReport();
        if (nativeReport != null && !nativeReport.isEmpty()) out.append(nativeReport);
        out.append("loader variant: ").append(loadedVariant).append('\n');
        out.append("loader born retained: ").append(bornRelativePath != null ? "yes" : "no").append('\n');
        out.append("loader idle retained: ").append(idleRelativePath != null ? "yes" : "no").append('\n');
        return out.toString();
    }

    static int loadedVariantForTest() {
        return loadedVariant;
    }

    static final class InitialFrame {
        final String atlasPlist;
        final String frameName;
        final int mode;

        InitialFrame(String atlasPlist, String frameName, int mode) {
            this.atlasPlist = atlasPlist;
            this.frameName = frameName;
            this.mode = mode;
        }
    }

    static final class CurrentFrame {
        final String atlasPlist;
        final String frameName;
        final int mode;
        final int frameIndex;

        CurrentFrame(String atlasPlist, String frameName, int mode, int frameIndex) {
            this.atlasPlist = atlasPlist;
            this.frameName = frameName;
            this.mode = mode;
            this.frameIndex = frameIndex;
        }
    }

    private static Pair resolve(File assetRoot, int variant) {
        try {
            File canonicalRoot = assetRoot.getCanonicalFile();
            if (!canonicalRoot.isDirectory()) return null;
            ArrayList<File> files = new ArrayList<>();
            if (!collectFiles(canonicalRoot, canonicalRoot, 0, files)) return null;

            String bornName = String.format(Locale.ROOT, "se01_born%02d.actData", variant);
            String idleName = String.format(Locale.ROOT, "se01_i%02d.actData", variant);
            File born = uniqueBasename(files, bornName);
            File idle = uniqueBasename(files, idleName);
            if (born == null || idle == null) return null;
            String bornRelative = relativePath(canonicalRoot, born);
            String idleRelative = relativePath(canonicalRoot, idle);
            if (bornRelative == null || idleRelative == null) return null;
            return new Pair(bornRelative, idleRelative);
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
        File match = null;
        for (File file : files) {
            if (!wanted.equalsIgnoreCase(file.getName())) continue;
            if (match != null) return null;
            match = file;
        }
        return match;
    }

    private static String relativePath(File root, File file) throws Exception {
        File canonicalRoot = root.getCanonicalFile();
        File canonicalFile = file.getCanonicalFile();
        if (!withinRoot(canonicalRoot, canonicalFile)) return null;
        String rootPath = canonicalRoot.getPath();
        String filePath = canonicalFile.getPath();
        if (filePath.equals(rootPath)) return null;
        return filePath.substring(rootPath.length() + 1).replace(File.separatorChar, '/');
    }

    private static boolean withinRoot(File root, File file) {
        String rootPath = root.getPath();
        String filePath = file.getPath();
        return filePath.equals(rootPath) || filePath.startsWith(rootPath + File.separator);
    }

    private static final class Pair {
        final String bornRelativePath;
        final String idleRelativePath;

        Pair(String bornRelativePath, String idleRelativePath) {
            this.bornRelativePath = bornRelativePath;
            this.idleRelativePath = idleRelativePath;
        }
    }
}
