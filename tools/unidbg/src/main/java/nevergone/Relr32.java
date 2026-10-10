package nevergone;

import com.alibaba.fastjson.JSON;
import com.alibaba.fastjson.JSONArray;
import com.alibaba.fastjson.JSONObject;
import com.github.unidbg.AndroidEmulator;
import com.github.unidbg.Module;
import com.github.unidbg.pointer.UnidbgPointer;
import java.io.File;
import java.util.Map;
import java.security.MessageDigest;
import java.nio.file.Files;
import java.nio.file.Paths;

/** Rebase DT_RELR targets ignored by this local unidbg version, before any native calls. */
final class Relr32 {
    static void apply(AndroidEmulator emulator, Map<String, File> selectedFiles) throws Exception {
        JSONArray plans = JSON.parseArray(new String(Files.readAllBytes(Paths.get("target/relr-plan.json")), "UTF-8"));
        int total = 0;
        for (int i = 0; i < plans.size(); i++) {
            JSONObject plan = plans.getJSONObject(i);
            Module module = emulator.getMemory().findModule(plan.getString("module"));
            if (module == null) continue;
            File source = selectedFiles.get(module.name);
            if (source == null) throw new IllegalStateException("Missing RELR source: " + module.name);
            byte[] digest = MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(source.toPath()));
            StringBuilder hash = new StringBuilder();
            for (byte b : digest) hash.append(String.format("%02x", b & 255));
            if (!hash.toString().equals(plan.getString("sha256")))
                throw new IllegalStateException("RELR source hash mismatch: " + module.name);
            JSONArray entries = plan.getJSONArray("entries");
            int patched = 0, already = 0;
            // Validate the whole module before changing any of its targets.
            for (int j = 0; j < entries.size(); j++) {
                JSONArray entry = entries.getJSONArray(j);
                long offset = entry.getLongValue(0), raw = entry.getLongValue(1);
                long live = Integer.toUnsignedLong(UnidbgPointer.pointer(emulator, module.base + offset).getInt(0));
                long expected = (raw + module.base) & 0xffffffffL;
                if (live != raw && live != expected) {
                    throw new IllegalStateException("Unexpected RELR target " + module.name + "+0x" + Long.toHexString(offset));
                }
            }
            for (int j = 0; j < entries.size(); j++) {
                JSONArray entry = entries.getJSONArray(j);
                long offset = entry.getLongValue(0), raw = entry.getLongValue(1);
                UnidbgPointer pointer = UnidbgPointer.pointer(emulator, module.base + offset);
                long live = Integer.toUnsignedLong(pointer.getInt(0));
                if (live == raw) {
                    pointer.setInt(0, (int) (module.base + raw));
                    patched++;
                } else already++;
            }
            total += patched;
            System.out.printf("RELR %s patched=%d already=%d%n", module.name, patched, already);
        }
        System.out.printf("RELR_PATCHED_TOTAL=%d%n", total);
    }
}
