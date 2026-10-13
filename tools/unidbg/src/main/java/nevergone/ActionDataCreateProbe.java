package nevergone;

import capstone.api.Instruction;
import com.alibaba.fastjson.JSONArray;
import com.alibaba.fastjson.JSONObject;
import com.github.unidbg.AndroidEmulator;
import com.github.unidbg.Module;

/**
 * Bounded direct-entry probe for allowlisted zero-argument dispatch create functions.
 *
 * The legacy class name is retained for CLI/source compatibility. CLI offsets are
 * Ghidra addresses. libcocos2dcpp.so is loaded in Ghidra at ELF/runtime module
 * offset + 0x10000, so the native call subtracts that bias. Appending `:fallback`
 * enables a bounded post-first-call R0 override so the native function follows its
 * own fallback path to the second virtual dispatch.
 */
final class ActionDataCreateProbe {
    private static final long GHIDRA_BIAS = 0x10000L;
    private static final long GHIDRA_CREATE_A = 0x29ec10L;
    private static final long GHIDRA_CREATE_B = 0x29ed0cL;
    private static final long GHIDRA_CREATE_C = 0x2ace4cL;
    private static final int PREFLIGHT_BYTES = 0x80;
    private static final int PREFLIGHT_MAX_INSTRUCTIONS = 64;
    private static final String FALLBACK_SUFFIX = ":fallback";

    static void run(AndroidEmulator emulator, Module module, String requestedAddress) {
        boolean fallbackPathRequested = requestedAddress != null && requestedAddress.endsWith(FALLBACK_SUFFIX);
        String addressText = fallbackPathRequested
                ? requestedAddress.substring(0, requestedAddress.length() - FALLBACK_SUFFIX.length())
                : requestedAddress;
        long ghidraAddress = parseAddress(addressText);
        if (ghidraAddress != GHIDRA_CREATE_A && ghidraAddress != GHIDRA_CREATE_B &&
                ghidraAddress != GHIDRA_CREATE_C)
            throw new IllegalArgumentException(String.format(
                    "Dispatch create probe Ghidra address is not allowlisted: 0x%x", ghidraAddress));
        if (ghidraAddress < GHIDRA_BIAS)
            throw new IllegalArgumentException("Ghidra address is below libcocos bias");
        long runtimeOffset = ghidraAddress - GHIDRA_BIAS;
        if (runtimeOffset < 0 || runtimeOffset >= module.size)
            throw new IllegalArgumentException(String.format(
                    "Dispatch create runtime offset outside module: 0x%x", runtimeOffset));

        // Each direct-entry workload starts natural. A fallback callback may switch
        // subsequent dispatch observations to interventional origin; finally reset
        // so no later workload can inherit that state.
        DispatchEvidenceContext.reset();
        emitPreflightDisassembly(emulator, module, ghidraAddress, runtimeOffset);
        if (fallbackPathRequested)
            ActionDataFallbackPathControl.install(emulator, module, emulator.attach(), addressText);

        JSONObject row = new JSONObject(true);
        row.put("schema", 4);
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
        row.put("expected_receiver_class_from_static_inference", expectedReceiverClass(ghidraAddress));
        row.put("allowlist_evidence", ghidraAddress == GHIDRA_CREATE_C
                ? "create_void_plus_static_receiver_vtable_exact_slot_candidates"
                : "create_void_plus_runtime_receiver_vtable_verified");
        row.put("fallback_path_requested", fallbackPathRequested);
        row.put("fallback_control", fallbackPathRequested
                ? "post_first_virtual_call_r0_zero_only" : "none");

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
        } finally {
            DispatchEvidenceContext.reset();
        }
    }

    private static String expectedReceiverClass(long ghidraAddress) {
        if (ghidraAddress == GHIDRA_CREATE_A) return "ActionData";
        if (ghidraAddress == GHIDRA_CREATE_B) return "EenemyActionDataSet";
        if (ghidraAddress == GHIDRA_CREATE_C) return "WeaponsItemData";
        return null;
    }

    private static void emitPreflightDisassembly(AndroidEmulator emulator, Module module,
                                                  long ghidraAddress, long runtimeOffset) {
        long runtimeAddress = module.base + runtimeOffset;
        int maxBytes = (int) Math.min(PREFLIGHT_BYTES, module.size - runtimeOffset);
        byte[] code = emulator.getBackend().mem_read(runtimeAddress, maxBytes);
        Instruction[] instructions = emulator.disassemble(
                runtimeAddress, code, true, PREFLIGHT_MAX_INSTRUCTIONS);

        JSONObject row = new JSONObject(true);
        row.put("schema", 1);
        row.put("kind", "action_data_create_preflight_disassembly");
        row.put("module", module.name);
        row.put("source_id", module.name + ":" + String.format("%08x", ghidraAddress));
        row.put("ghidra_address", String.format("0x%x", ghidraAddress));
        row.put("runtime_offset", String.format("0x%x", runtimeOffset));
        row.put("byte_limit", maxBytes);
        row.put("instruction_limit", PREFLIGHT_MAX_INSTRUCTIONS);

        JSONArray decoded = new JSONArray();
        JSONObject byGhidraAddress = new JSONObject(true);
        if (instructions != null) {
            for (Instruction instruction : instructions) {
                long instructionRuntimeOffset = instruction.getAddress() - module.base;
                if (instructionRuntimeOffset < runtimeOffset ||
                        instructionRuntimeOffset >= runtimeOffset + maxBytes)
                    continue;
                long instructionGhidraAddress = instructionRuntimeOffset + GHIDRA_BIAS;
                JSONObject item = new JSONObject(true);
                item.put("ghidra_address", String.format("0x%x", instructionGhidraAddress));
                item.put("runtime_offset", String.format("0x%x", instructionRuntimeOffset));
                item.put("mnemonic", instruction.getMnemonic());
                item.put("operands", instruction.getOpStr());
                decoded.add(item);
                byGhidraAddress.put(String.format("0x%x", instructionGhidraAddress),
                        instruction.getMnemonic() +
                                ((instruction.getOpStr() == null || instruction.getOpStr().isEmpty())
                                        ? "" : " " + instruction.getOpStr()));
            }
        }
        row.put("decoded_instruction_count", decoded.size());
        row.put("instructions", decoded);

        JSONArray expectedSites = new JSONArray();
        long[] sites;
        if (ghidraAddress == GHIDRA_CREATE_A) {
            sites = new long[] {0x29ec26L, 0x29ec3cL};
        } else if (ghidraAddress == GHIDRA_CREATE_B) {
            sites = new long[] {0x29ed22L, 0x29ed38L};
        } else {
            sites = new long[] {0x2ace64L, 0x2ace7aL};
        }
        for (long site : sites) {
            JSONObject siteRow = new JSONObject(true);
            String key = String.format("0x%x", site);
            siteRow.put("ghidra_address", key);
            siteRow.put("runtime_offset", String.format("0x%x", site - GHIDRA_BIAS));
            String decodedText = byGhidraAddress.getString(key);
            siteRow.put("instruction_boundary", decodedText != null);
            siteRow.put("instruction", decodedText);
            siteRow.put("is_blx", decodedText != null && decodedText.startsWith("blx"));
            expectedSites.add(siteRow);
        }
        row.put("expected_dispatch_sites", expectedSites);
        EvidenceProbe.emit(row);
    }

    private static long parseAddress(String value) {
        if (value == null || !value.matches("0x[0-9a-fA-F]+"))
            throw new IllegalArgumentException("Expected hexadecimal Ghidra address, got: " + value);
        return Long.parseLong(value.substring(2), 16);
    }

    private ActionDataCreateProbe() {}
}
