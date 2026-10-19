package org.nevergone.recomp;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

public final class PlayerActionAssetResolverSmoke {
    private static void write(File file, String text) throws Exception {
        File parent = file.getParentFile();
        if (parent != null) parent.mkdirs();
        Files.write(file.toPath(), text.getBytes(StandardCharsets.UTF_8));
    }

    private static void writeBundle(File file, String[][] entries) throws Exception {
        File parent = file.getParentFile();
        if (parent != null) parent.mkdirs();
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        for (String[] entry : entries) {
            byte[] payload = entry[1].getBytes(StandardCharsets.UTF_8);
            out.write(entry[0].getBytes(StandardCharsets.US_ASCII));
            out.write('\n');
            out.write(Integer.toString(payload.length).getBytes(StandardCharsets.US_ASCII));
            out.write('\n');
            out.write(payload);
            out.write('\n');
        }
        Files.write(file.toPath(), out.toByteArray());
    }

    private static String plist(String initialActionName) {
        return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
                "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" " +
                "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n" +
                "<plist version=\"1.0\"><dict>" +
                "<key>action01</key><dict><key>name</key><string>idle0.actData</string>" +
                "<key>looping</key><true/></dict>" +
                "<key>action03</key><dict><key>name</key><string>attack_a.actData</string>" +
                "<key>looping</key><true/></dict>" +
                "<key>action18</key><dict><key>name</key><string>block_b.actData</string>" +
                "<key>looping</key><true/></dict>" +
                "<key>action28</key><dict><key>name</key><string>" + initialActionName +
                "</string><key>looping</key><true/></dict>" +
                "<key>action30</key><dict><key>name</key><string>b_fallback.actData</string>" +
                "<key>looping</key><true/></dict>" +
                "<key>action33</key><dict><key>name</key><string>jump_c.actData</string>" +
                "<key>looping</key><true/></dict>" +
                "</dict></plist>\n";
    }

    private static String[][] defaultBundleEntries() {
        return new String[][] {
                {"p01_decoy.actData", "decoy"},
                {"attack_a.actData", "a"},
                {"block_b.actData", "b"},
                {"p01_i01.actData", "initial"},
                {"b_fallback.actData", "fallback"},
                {"jump_c.actData", "jump"},
        };
    }

    public static void main(String[] args) throws Exception {
        File root = Files.createTempDirectory("nevergone-player-action-resolver").toFile();
        try {
            File plist = new File(root, "configs/player/P01_ACTIONS.PLIST");
            File bundle = new File(root,
                    "player/actions_data/adata/p01/normal/player_01.actionsData");
            write(plist, plist("p01_i01.actData"));
            writeBundle(bundle, defaultBundleEntries());

            // MEPlayer::createActions builds its CCArray from action%02d in
            // order, and setPlayerStateWithIdx indexes that array directly by
            // MEPLAYERSTATE. Therefore key = action(state + 1).
            if (!"action01".equals(PlayerActionAssetResolver.actionKeyForState(0)) ||
                    !"action03".equals(PlayerActionAssetResolver.actionKeyForState(0x02)) ||
                    !"action18".equals(PlayerActionAssetResolver.actionKeyForState(0x11)) ||
                    !"action28".equals(PlayerActionAssetResolver.actionKeyForState(0x1b)) ||
                    !"action30".equals(PlayerActionAssetResolver.actionKeyForState(0x1d)) ||
                    !"action33".equals(PlayerActionAssetResolver.actionKeyForState(0x20)) ||
                    !"action68".equals(PlayerActionAssetResolver.actionKeyForState(0x43)) ||
                    PlayerActionAssetResolver.actionKeyForState(-1) != null ||
                    PlayerActionAssetResolver.actionKeyForState(0x44) != null) {
                throw new AssertionError("MEPLAYERSTATE to action key mapping mismatch");
            }

            PlayerActionAssetResolver.Result result =
                    PlayerActionAssetResolver.resolveInitial(root, 1);
            if (result == null || result.career != 1 || !"action28".equals(result.actionKey) ||
                    !"configs/player/P01_ACTIONS.PLIST".equals(result.actionsPlistRelativePath) ||
                    !"p01_i01.actData".equals(result.actionName) ||
                    result.actionDataRelativePath != null || !result.isEmbedded() ||
                    !"player/actions_data/adata/p01/normal/player_01.actionsData".equals(
                            result.actionBundleRelativePath) ||
                    !"p01_i01.actData".equals(result.actionBundleEntryName)) {
                throw new AssertionError("failed to resolve embedded initial state 0x1b/action28");
            }

            result = PlayerActionAssetResolver.resolveState(root, 1, 0x02);
            if (result == null || !"action03".equals(result.actionKey) ||
                    !"attack_a.actData".equals(result.actionBundleEntryName)) {
                throw new AssertionError("A state 0x02 did not resolve action03");
            }

            result = PlayerActionAssetResolver.resolveState(root, 1, 0x11);
            if (result == null || !"action18".equals(result.actionKey) ||
                    !"block_b.actData".equals(result.actionBundleEntryName)) {
                throw new AssertionError("B state 0x11 did not resolve action18");
            }

            result = PlayerActionAssetResolver.resolveState(root, 1, 0x1d);
            if (result == null || !"action30".equals(result.actionKey) ||
                    !"b_fallback.actData".equals(result.actionBundleEntryName)) {
                throw new AssertionError("B fallback state 0x1d did not resolve action30");
            }

            result = PlayerActionAssetResolver.resolveState(root, 1, 0x20);
            if (result == null || !"action33".equals(result.actionKey) ||
                    !"jump_c.actData".equals(result.actionBundleEntryName)) {
                throw new AssertionError("C/jump state 0x20 did not resolve action33");
            }

            if (PlayerActionAssetResolver.resolveInitial(root, 0) != null ||
                    PlayerActionAssetResolver.resolveState(root, 1, -1) != null ||
                    PlayerActionAssetResolver.resolveState(root, 1, 0x44) != null ||
                    PlayerActionAssetResolver.resolve(root, 1, "action00") != null ||
                    PlayerActionAssetResolver.resolve(root, 1, "bad") != null) {
                throw new AssertionError("invalid career/action key accepted");
            }

            File duplicateBundle = new File(root, "duplicate/player_01.actionsData");
            writeBundle(duplicateBundle, new String[][] {{"p01_i01.actData", "duplicate"}});
            if (PlayerActionAssetResolver.resolveInitial(root, 1) != null) {
                throw new AssertionError("ambiguous actionsData bundle accepted");
            }
            duplicateBundle.delete();

            writeBundle(bundle, new String[][] {{"other.actData", "missing-target"}});
            if (PlayerActionAssetResolver.resolveInitial(root, 1) != null) {
                throw new AssertionError("bundle missing requested entry accepted");
            }
            bundle.delete();

            File action = new File(root, "player/actions/player_start.actData");
            write(plist, plist("folder/player_start.actData"));
            write(action, "synthetic");
            result = PlayerActionAssetResolver.resolveInitial(root, 1);
            if (result == null || result.isEmbedded() ||
                    !"player/actions/player_start.actData".equals(result.actionDataRelativePath)) {
                throw new AssertionError("standalone action-data fallback failed");
            }

            File duplicate = new File(root, "duplicate/player_start.actData");
            write(duplicate, "duplicate");
            if (PlayerActionAssetResolver.resolveInitial(root, 1) != null) {
                throw new AssertionError("ambiguous standalone action-data basename accepted");
            }
            duplicate.delete();

            File duplicatePlist = new File(root, "duplicate/p01_actions.plist");
            write(duplicatePlist, plist("player_start.actData"));
            if (PlayerActionAssetResolver.resolveInitial(root, 1) != null) {
                throw new AssertionError("ambiguous actions plist accepted");
            }
            duplicatePlist.delete();

            write(plist, plist("missing.actData"));
            if (PlayerActionAssetResolver.resolveInitial(root, 1) != null) {
                throw new AssertionError("missing action data accepted");
            }
        } finally {
            deleteTree(root);
        }
    }

    private static void deleteTree(File file) {
        if (file == null || !file.exists()) return;
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children != null) for (File child : children) deleteTree(child);
        }
        file.delete();
    }
}
