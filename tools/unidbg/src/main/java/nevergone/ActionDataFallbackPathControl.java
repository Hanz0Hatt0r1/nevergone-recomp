package nevergone;

import com.alibaba.fastjson.JSONObject;
import com.github.unidbg.AndroidEmulator;
import com.github.unidbg.Emulator;
import com.github.unidbg.Module;
import com.github.unidbg.debugger.BreakPointCallback;
import com.github.unidbg.debugger.Debugger;
import unicorn.ArmConst;

/**
 * Bounded path-control for the two allowlisted ActionData create functions.
 *
 * The first virtual call is allowed to execute normally. At the instruction
 * immediately after that BLX returns, this control records the native return
 * value and replaces only R0 with zero. The original create() code then follows
 * its own failure/fallback branch and reaches the second virtual dispatch slot.
 * No call target, vtable entry, object memory, PC, LR, or stack state is changed.
 */
final class ActionDataFallbackPathControl {
    private static final long GHIDRA_BIAS = 0x10000L;
    private static final long GHIDRA_CREATE_A = 0x29ec10L;
    private static final long GHIDRA_CREATE_B = 0x29ed0cL;
    private static final long GHIDRA_FIRST_CALL_A = 0x29ec26L;
    private static final long GHIDRA_FIRST_CALL_B = 0x29ed22L;

    static void install(AndroidEmulator emulator, Module module, Debugger debugger, String requestedAddress) {
        long createGhidra = parseAddress(requestedAddress);
        final long firstCallGhidra;
        if (createGhidra == GHIDRA_CREATE_A) firstCallGhidra = GHIDRA_FIRST_CALL_A;
        else if (createGhidra == GHIDRA_CREATE_B) firstCallGhidra = GHIDRA_FIRST_CALL_B;
        else throw new IllegalArgumentException(String.format(
                "ActionData fallback probe Ghidra address is not allowlisted: 0x%x", createGhidra));

        // Both allowlisted first dispatches are verified 16-bit Thumb `blx r3`.
        final long postCallGhidra = firstCallGhidra + 2L;
        final long postCallOffset = postCallGhidra - GHIDRA_BIAS;
        if (postCallOffset < 0 || postCallOffset >= module.size)
            throw new IllegalArgumentException("Fallback post-call offset outside module");

        final boolean[] applied = {false};
        debugger.addBreakPoint((module.base + postCallOffset) | 1L, new BreakPointCallback() {
            @Override public boolean onHit(Emulator<?> ignored, long callbackAddress) {
                if (applied[0]) return true;
                applied[0] = true;

                long originalR0 = Integer.toUnsignedLong(
                        emulator.getBackend().reg_read(ArmConst.UC_ARM_REG_R0).intValue());
                emulator.getBackend().reg_write(ArmConst.UC_ARM_REG_R0, 0);

                JSONObject row = new JSONObject(true);
                row.put("schema", 1);
                row.put("kind", "action_data_fallback_return_override");
                row.put("module", module.name);
                row.put("source_id", module.name + ":" + String.format("%08x", createGhidra));
                row.put("ghidra_address_bias", String.format("0x%x", GHIDRA_BIAS));
                row.put("first_dispatch_ghidra_address", String.format("0x%x", firstCallGhidra));
                row.put("post_call_ghidra_address", String.format("0x%x", postCallGhidra));
                row.put("post_call_runtime_offset", String.format("0x%x", postCallOffset));
                row.put("callback_address", String.format("0x%x", callbackAddress));
                row.put("original_return_r0", String.format("0x%08x", originalR0));
                row.put("forced_return_r0", "0x00000000");
                row.put("mutation_scope", "r0_only_after_first_virtual_call_return");
                row.put("first_virtual_call_executed_normally", true);
                row.put("call_target_modified", false);
                row.put("receiver_memory_modified", false);
                EvidenceProbe.emit(row);
                return true;
            }
        });

        System.out.printf("ACTION_DATA_FALLBACK_CONTROL source=0x%x post_call=0x%x%n",
                createGhidra, postCallGhidra);
    }

    private static long parseAddress(String value) {
        if (value == null || !value.matches("0x[0-9a-fA-F]+"))
            throw new IllegalArgumentException("Expected hexadecimal Ghidra address, got: " + value);
        return Long.parseLong(value.substring(2), 16);
    }

    private ActionDataFallbackPathControl() {}
}
