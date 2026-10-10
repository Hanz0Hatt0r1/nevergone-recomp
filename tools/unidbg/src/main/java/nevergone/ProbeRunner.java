package nevergone;

import com.alibaba.fastjson.JSON;
import com.alibaba.fastjson.JSONArray;
import com.alibaba.fastjson.JSONObject;
import com.github.unidbg.AndroidEmulator;
import com.github.unidbg.Module;
import com.github.unidbg.memory.MemoryBlock;
import com.github.unidbg.pointer.UnidbgPointer;
import unicorn.ArmConst;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

/** Bounded, declarative native calls. No game object or filesystem state is synthesized. */
final class ProbeRunner {
    private static final String ENCODE = "_ZN12GameSaveData6EncodeEPciS0_j";
    private static final String DECODE = "_ZN12GameSaveData6DecodeEPciS0_j";

    private static JSONObject registers(AndroidEmulator emulator) {
        JSONObject out = new JSONObject(true);
        for (int i = 0; i < 13; i++)
            out.put("r" + i, String.format("0x%08x", emulator.getBackend().reg_read(ArmConst.UC_ARM_REG_R0 + i).intValue()));
        out.put("sp", String.format("0x%08x", emulator.getBackend().reg_read(ArmConst.UC_ARM_REG_SP).intValue()));
        out.put("lr", String.format("0x%08x", emulator.getBackend().reg_read(ArmConst.UC_ARM_REG_LR).intValue()));
        out.put("pc", String.format("0x%08x", emulator.getBackend().reg_read(ArmConst.UC_ARM_REG_PC).intValue()));
        return out;
    }

    static void run(AndroidEmulator emulator, Module module, File planFile) throws Exception {
        JSONArray plan = JSON.parseArray(new String(Files.readAllBytes(planFile.toPath()), StandardCharsets.UTF_8));
        if (plan == null || plan.isEmpty() || plan.size() > 256) throw new IllegalArgumentException("Expected 1..256 probes");
        for (int index = 0; index < plan.size(); index++) {
            JSONObject spec = plan.getJSONObject(index);
            String symbol = spec.getString("symbol");
            if (!ENCODE.equals(symbol) && !DECODE.equals(symbol))
                throw new IllegalArgumentException("Unsafe or unknown probe symbol: " + symbol);
            long offset = ENCODE.equals(symbol) ? 0x33a766L : 0x33a788L;
            if (!String.format("0x%x", offset).equals(spec.getString("offset")))
                throw new IllegalArgumentException("Offset mismatch for " + symbol);
            if (module.findSymbolByName(symbol, false) == null ||
                    ((module.findSymbolByName(symbol, false).getAddress() - module.base) & ~1L) != offset)
                throw new IllegalStateException("Runtime symbol address mismatch: " + symbol);
            byte[] input = hexBytes(spec.getString("input_hex"));
            if (input.length > 4095) throw new IllegalArgumentException("Probe input exceeds 4095 bytes");
            long key = spec.getLongValue("key");
            if (key < 0 || key > 0xffffffffL) throw new IllegalArgumentException("Key outside uint32");
            MemoryBlock block = emulator.getMemory().malloc(8192, true);
            try {
                UnidbgPointer source = block.getPointer();
                UnidbgPointer destination = source.share(4096, 0);
                source.write(0, input, 0, input.length);
                destination.setByte(input.length, (byte) 0x5a);
                JSONObject row = new JSONObject(true);
                row.put("schema", 1); row.put("kind", "probe_symbol"); row.put("case", index);
                row.put("module", module.name); row.put("module_base", String.format("0x%x", module.base));
                row.put("symbol", symbol); row.put("offset", spec.getString("offset")); row.put("arch", "ARM32 Thumb");
                row.put("args", new Object[] {"null_this", "source", input.length, "destination", key});
                row.put("key", key); row.put("memory_before", new JSONObject(true) {{
                    put("source_hex", EvidenceProbe.hex(input)); put("destination_hex", EvidenceProbe.hex(destination.getByteArray(0, input.length)));
                    put("guard", destination.getByte(input.length) & 255);
                }});
                row.put("registers_before_call", registers(emulator));
                Number result = module.callFunction(emulator, symbol, 0, source, input.length, destination, (int) key);
                row.put("registers_after_call", registers(emulator));
                row.put("return_value", result.intValue());
                JSONObject after = new JSONObject(true);
                after.put("source_hex", EvidenceProbe.hex(source.getByteArray(0, input.length)));
                after.put("destination_hex", EvidenceProbe.hex(destination.getByteArray(0, input.length)));
                after.put("guard", destination.getByte(input.length) & 255);
                row.put("memory_after", after);
                JSONObject trace = new JSONObject(true); trace.put("mode", "none"); trace.put("reason", "bounded leaf function; no instruction hook");
                row.put("trace", trace);
                EvidenceProbe.emit(row);
            } finally { block.free(); }
        }
    }

    private static byte[] hexBytes(String value) {
        if (value == null || (value.length() & 1) != 0 || !value.matches("[0-9a-fA-F]*"))
            throw new IllegalArgumentException("Invalid input_hex");
        byte[] out = new byte[value.length() / 2];
        for (int i = 0; i < out.length; i++) out[i] = (byte) Integer.parseInt(value.substring(i * 2, i * 2 + 2), 16);
        return out;
    }
}
