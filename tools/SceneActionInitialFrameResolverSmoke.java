package org.nevergone.recomp;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

public final class SceneActionInitialFrameResolverSmoke {
    private static void write(File file, String value) throws Exception {
        File parent = file.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IllegalStateException("mkdir failed: " + parent);
        }
        Files.write(file.toPath(), value.getBytes(StandardCharsets.UTF_8));
    }

    private static String plist() {
        return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
                "<plist version=\"1.0\"><dict>" +
                "<key>frames</key><dict>" +
                "<key>klhuo_01.png</key><dict>" +
                "<key>x</key><integer>4</integer>" +
                "<key>y</key><integer>5</integer>" +
                "<key>width</key><integer>16</integer>" +
                "<key>height</key><integer>20</integer>" +
                "<key>offsetX</key><integer>0</integer>" +
                "<key>offsetY</key><integer>0</integer>" +
                "<key>originalWidth</key><integer>16</integer>" +
                "<key>originalHeight</key><integer>20</integer>" +
                "</dict></dict></dict></plist>";
    }

    public static void main(String[] args) throws Exception {
        File root = Files.createTempDirectory("scene-action-frame-resolver").toFile();
        try {
            File gamescene = new File(root, "gamescene");
            write(new File(gamescene, "scene_action.plist"), plist());
            write(new File(gamescene, "scene_action.png"), "texture-placeholder");

            SceneActionPairRuntimeLoader.InitialFrame request =
                    new SceneActionPairRuntimeLoader.InitialFrame(
                            "scene_action.plist", "klhuo_01.png", 0);
            SceneActionInitialFrameResolver.Result resolved =
                    SceneActionInitialFrameResolver.resolve(root, request);
            assert resolved != null;
            assert resolved.mode == 0;
            assert "gamescene/scene_action.plist".equals(resolved.frame.plistRelativePath);
            assert "gamescene/scene_action.png".equals(resolved.frame.textureRelativePath);
            assert "klhuo_01.png".equals(resolved.frame.requestedName);
            assert resolved.frame.textureX == 4;
            assert resolved.frame.textureY == 5;
            assert resolved.frame.textureWidth == 16;
            assert resolved.frame.textureHeight == 20;

            assert SceneActionInitialFrameResolver.resolve(
                    root,
                    new SceneActionPairRuntimeLoader.InitialFrame(
                            "scene_action.plist", "missing.png", 0)) == null;
            assert SceneActionInitialFrameResolver.resolve(
                    root,
                    new SceneActionPairRuntimeLoader.InitialFrame(
                            "scene_action.plist", "klhuo_01.png", 2)) == null;
            assert SceneActionInitialFrameResolver.resolve(null, request) == null;
        } finally {
            deleteRecursively(root);
        }
    }

    private static void deleteRecursively(File file) {
        if (file == null || !file.exists()) return;
        File[] children = file.listFiles();
        if (children != null) {
            for (File child : children) deleteRecursively(child);
        }
        file.delete();
    }
}
