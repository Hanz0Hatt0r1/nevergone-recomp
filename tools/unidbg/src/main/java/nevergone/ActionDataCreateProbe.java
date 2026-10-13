package nevergone;

import com.alibaba.fastjson.JSONObject;
import com.github.unidbg.AndroidEmulator;
import com.github.unidbg.Module;

/**
 * Bounded direct-entry probe for the two zero-argument ActionData create functions
 * selected by static dispatch-equivalence analysis.
 *
 * Only the two audited offsets are accepted. The call is made with no guest
 * arguments and the Thumb bit is set explicitly on the numeric module offset.
 */
final class ActionDataCreateProbe {
    private static final long CREATE_A = 0x29ec10L;
    private static final long CREATE_B = 0x29ed0cL;

    static void run(AndroidEmulator emulator, Module module, String requestedOffset) {
        long offset = parseOffset(requestedOffset);
        if (offset != CREATE_A && offset != CREATE_B)
            throw new IllegalArgumentException(String.format(
                    "ActionData create probe offset is not allowlisted: 0x%x", offset));
        if (offset < 0 || offset >= module.size)
            throw new IllegalArgumentException(String.format(
                    "ActionData create probe offset outside module: 0x%x", offset));

        JSONObject row = new JSONObject(true);
        row.put("schema", 1);
        row.put("kind", "action_data_create_entry");
        row.put("module", module.name);
        row.put("module_base", String.format("0x%x", module.base));
        row.put("source_id", module.name + ":" + String.format("%08x", offset));
        row.put("offset", String.format("0x%x", offset));
        row.put("signature_evidence", "undefined __stdcall create(void)");
        row.put("guest_argument_count", 0);
        row.put("thumb_entry_offset", String.format("0x%x", offset | 1L));

        try {
            Number result = module.callFunction(emulator, offset | 1L);
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

    private static long parseOffset(String value) {
        if (value == null || !value.matches("0x[0-9a-fA-F]+"))
            throw new IllegalArgumentException("Expected hexadecimal module offset, got: " + value);
        return Long.parseLong(value.substring(2), 16);
    }

    private ActionDataCreateProbe() {}
}
