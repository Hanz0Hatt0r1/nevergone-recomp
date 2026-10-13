package nevergone;

import com.alibaba.fastjson.JSONObject;
import com.github.unidbg.AndroidEmulator;
import com.github.unidbg.Module;

/**
 * Bounded direct-entry probe for the two zero-argument ActionData create functions
 * selected by static dispatch-equivalence analysis.
 *
 * CLI offsets are Ghidra addresses. libcocos2dcpp.so is loaded in Ghidra at
 * ELF/runtime module offset + 0x10000, so the native call subtracts that bias.
 */
final class ActionDataCreateProbe {
    private static final long GHIDRA_BIAS = 0x10000L;
    private static final long GHIDRA_CREATE_A = 0x29ec10L;
    private static final long GHIDRA_CREATE_B = 0x29ed0cL;

    static void run(AndroidEmulator emulator, Module module, String requestedAddress) {
        long ghidraAddress = parseAddress(requestedAddress);
        if (ghidraAddress != GHIDRA_CREATE_A && ghidraAddress != GHIDRA_CREATE_B)
            throw new IllegalArgumentException(String.format(
                    "ActionData create probe Ghidra address is not allowlisted: 0x%x", ghidraAddress));
        if (ghidraAddress < GHIDRA_BIAS)
            throw new IllegalArgumentException("Ghidra address is below libcocos bias");
        long runtimeOffset = ghidraAddress - GHIDRA_BIAS;
        if (runtimeOffset < 0 || runtimeOffset >= module.size)
            throw new IllegalArgumentException(String.format(
                    "ActionData create runtime offset outside module: 0x%x", runtimeOffset));

        JSONObject row = new JSONObject(true);
        row.put("schema", 2);
        row.put("kind", "action_data_create_entry");
        row.put("module", module.name);
        row.put("module_base", String.format("0x%x", module.base));
        row.put("source_id", module.name + ":" + String.format("%08x", ghidraAddress));
        row.put("ghidra_address", String.format("0x%x", ghidraAddress));
        row.put("ghidra_address_bias", String.format("0x%x", GHIDRA_BIAS));
        row.put("runtime_offset", String.format("0x%x", runtimeOffset));
        row.put("signature_evidence", "undefined __stdcall create(void)");
        row.put("guest_argument_count", 0);
        row.put("thumb_entry_offset", String.format("0x%x", runtimeOffset | 1L));

        try {
            Number result = module.callFunction(emulator, runtimeOffset | 1L);
            long raw = Integer.toUnsignedLong(result.intValue());
            row.put("status", "returned");
            row.put("return_raw", String.format("0x%08x", raw));
            row.put("return_nonzero", raw != 0);
            EvidenceProbe.emit(row);
        } catch (Throwable error) {
            row.put("status", "error");
            row.put("error_type", error.getClass().getName());
            row.put("error", String.valueOf(error.getMessage()));
            EvidenceProbe.emit(row);
            if (error instanceof RuntimeException) throw (RuntimeException) error;
            if (error instanceof Error) throw (Error) error;
            throw new RuntimeException(error);
        }
    }

    private static long parseAddress(String value) {
        if (value == null || !value.matches("0x[0-9a-fA-F]+"))
            throw new IllegalArgumentException("Expected hexadecimal Ghidra address, got: " + value);
        return Long.parseLong(value.substring(2), 16);
    }

    private ActionDataCreateProbe() {}
}
