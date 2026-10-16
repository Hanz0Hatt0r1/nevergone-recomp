package org.nevergone.recomp;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.List;

public final class PlayerActionAtlasCacheResolverSmoke {
    private static void write(File file, String content) throws Exception {
        File parent = file.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IllegalStateException("mkdir failed: " + parent);
        }
        Files.write(file.toPath(), content.getBytes(StandardCharsets.UTF_8));
    }

    private static String plist(String texture) {
        return "<?xml version=\"1.0\"?><plist version=\"1.0\"><dict>" +
                "<key>frames</key><dict></dict>" +
                "<key>metadata</key><dict>" +
                "<key>textureFileName</key><string>" + texture + "</string>" +
                "</dict></dict></plist>";
    }

    public static void main(String[] args) throws Exception {
        File root = Files.createTempDirectory("nevergone-player-atlas-cache-smoke").toFile();
        File assets = new File(root, "assets");
        File player = new File(assets, "player/Player01_Res");

        // Recovered preload order is numeric and missing family members are not
        // replaced by an unrelated directory scan.
        write(new File(player, "p01_02_01_default.plist"),
                plist("p01_02_01_default.png"));
        write(new File(player, "p01_02_01_default.png"), "png");
        write(new File(player, "p01_02_03_default.plist"),
                plist("p01_02_03_default.png"));
        write(new File(player, "p01_02_03_default.png"), "png");
        write(new File(player, "p01_99_01_default.plist"),
                plist("p01_99_01_default.png"));
        write(new File(player, "p01_99_01_default.png"), "png");

        List<GameSceneAtlasResolver.Atlas> family =
                PlayerActionAtlasCacheResolver.resolve(assets, "p01_02_default.plist");
        assert family != null;
        assert family.size() == 2;
        assert family.get(0).plistRelativePath.equals(
                "player/Player01_Res/p01_02_01_default.plist");
        assert family.get(1).plistRelativePath.equals(
                "player/Player01_Res/p01_02_03_default.plist");

        // The action-specific plist is appended after the createActions preload
        // family, matching the observed native call order.
        write(new File(player, "p01_02_default.plist"), plist("p01_02_default.png"));
        write(new File(player, "p01_02_default.png"), "png");
        List<GameSceneAtlasResolver.Atlas> withExact =
                PlayerActionAtlasCacheResolver.resolve(assets, "p01_02_default.plist");
        assert withExact != null;
        assert withExact.size() == 3;
        assert withExact.get(2).plistRelativePath.equals(
                "player/Player01_Res/p01_02_default.plist");

        // Non-family action names remain exact-only; the helper does not invent
        // preload candidates for unrelated plist naming schemes.
        write(new File(assets, "enemy/T_E01_default.plist"), plist("T_E01_default.png"));
        write(new File(assets, "enemy/T_E01_default.png"), "png");
        List<GameSceneAtlasResolver.Atlas> exactOnly =
                PlayerActionAtlasCacheResolver.resolve(assets, "T_E01_default.plist");
        assert exactOnly != null && exactOnly.size() == 1;
        assert exactOnly.get(0).plistRelativePath.equals("enemy/T_E01_default.plist");

        assert PlayerActionAtlasCacheResolver.resolve(
                assets, "p01_77_default.plist") == null;
        assert PlayerActionAtlasCacheResolver.resolve(
                assets, "../p01_02_default.plist") == null;

        deleteTree(root);
    }

    private static void deleteTree(File file) {
        File[] children = file.listFiles();
        if (children != null) for (File child : children) deleteTree(child);
        if (!file.delete()) throw new IllegalStateException("delete failed: " + file);
    }
}
