package nevergone;

import capstone.api.Instruction;
import capstone.api.arm.OpInfo;
import capstone.api.arm.Operand;
import com.alibaba.fastjson.JSON;
import com.alibaba.fastjson.JSONArray;
import com.alibaba.fastjson.JSONObject;
import com.github.unidbg.AndroidEmulator;
import com.github.unidbg.Emulator;
import com.github.unidbg.Module;
import com.github.unidbg.arm.context.RegisterContext;
import com.github.unidbg.debugger.BreakPointCallback;
import com.github.unidbg.debugger.Debugger;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.HashSet;
import java.util.Set;

/** Observe a bounded allowlist of ARM32 Thumb CALLIND sites. */
final class DispatchTargetRuntimeTrace {
    private static final int MAX_SITES = 128;
    private static final long LIBCOCOS_GHIDRA_BIAS = 0x10000L;
    private static final String PLAN_FORMAT = "NEVERGONE.DispatchRuntimeProbePlan/2";

    static int install(AndroidEmulator emulator, Module module, File planFile) throws Exception {
        JSONArray plan = JSON.parseArray(new String(Files.readAllBytes(planFile.toPath()), StandardCharsets.UTF_8));
        if (plan == null || plan.isEmpty() || plan.size() > MAX_SITES)
            throw new IllegalArgumentException("Expected 1.." + MAX_SITES + " dispatch trace sites");

        /*
         * BreakPointCallback dispatch is owned by AbstractARMDebugger.breakMap.
         * Installing directly through Backend.addBreakPoint() creates the low-level
         * Unicorn breakpoint but bypasses that registry, so callbacks are never
         * recovered on break. Always install through Debugger and use an odd
         * address so the debugger records the breakpoint as Thumb.
         */
        Debugger debugger = emulator.attach();

        Set<Long> offsets = new HashSet<>();
        for (int index = 0; index < plan.size(); index++) {
            final JSONObject spec = plan.getJSONObject(index);
            if (!PLAN_FORMAT.equals(spec.getString("format")))
                throw new IllegalArgumentException(
                        "Stale/unsupported dispatch runtime plan; rebuild with current builder: " + spec.getString("format"));

            String sourceId = spec.getString("source_id");
            String expectedPrefix = module.name + ":";
            if (sourceId == null || !sourceId.startsWith(expectedPrefix))
                throw new IllegalArgumentException("Plan source_id outside " + module.name + ": " + sourceId);

            final long offset = parseOffset(spec.getString("call_site_offset"));
            final long ghidraAddress = parseOffset(spec.getString("ghidra_call_site_address"));
            final long ghidraBias = parseOffset(spec.getString("ghidra_address_bias"));
            if (ghidraBias != LIBCOCOS_GHIDRA_BIAS)
                throw new IllegalArgumentException(String.format(
                        "Unexpected libcocos Ghidra bias: expected 0x%x got 0x%x",
                        LIBCOCOS_GHIDRA_BIAS, ghidraBias));
            if (ghidraAddress < ghidraBias || ghidraAddress - ghidraBias != offset)
                throw new IllegalArgumentException(String.format(
                        "Inconsistent Ghidra/runtime call-site mapping: ghidra=0x%x bias=0x%x runtime=0x%x",
                        ghidraAddress, ghidraBias, offset));
            if (offset < 0 || offset >= module.size)
                throw new IllegalArgumentException("Call-site offset outside module: 0x" + Long.toHexString(offset));
            if (!offsets.add(offset))
                throw new IllegalArgumentException("Duplicate call-site offset: 0x" + Long.toHexString(offset));

            final long breakpointAddress = module.base + offset;
            debugger.addBreakPoint(breakpointAddress | 1L, new BreakPointCallback() {
                @Override public boolean onHit(Emulator<?> ignored, long address) {
                    try {
                        emitHit(emulator, module, spec, offset, address);
                    } catch (Throwable error) {
                        JSONObject row = baseRow(module, spec, offset);
                        row.put("kind", "dispatch_runtime_target_error");
                        row.put("error_type", error.getClass().getName());
                        row.put("error", String.valueOf(error.getMessage()));
                        EvidenceProbe.emit(row);
                    }
                    return true;
                }
            });
        }
        System.out.printf("DISPATCH_TRACE_SITES=%d%n", offsets.size());
        return offsets.size();
    }

    private static void emitHit(AndroidEmulator emulator, Module module, JSONObject spec,
                                long offset, long callbackAddress) {
        long pc = module.base + offset;
        byte[] code = emulator.getBackend().mem_read(pc, 4);
        Instruction[] instructions = emulator.disassemble(pc, code, true, 1);
        if (instructions == null || instructions.length == 0)
            throw new IllegalStateException("Unable to disassemble CALLIND site");
        Instruction instruction = instructions[0];
        if (!"blx".equals(instruction.getMnemonic()))
            throw new IllegalStateException("Expected Thumb BLX register call, got " +
                    instruction.getMnemonic() + " " + instruction.getOpStr());

        OpInfo operands = (OpInfo) instruction.getOperands();
        Operand[] values = operands.getOperands();
        if (values.length < 1 || values[0].getType() != capstone.Arm_const.ARM_OP_REG)
            throw new IllegalStateException("Expected BLX register operand at 0x" + Long.toHexString(offset));

        RegisterContext context = emulator.getContext();
        int unicornReg = instruction.mapToUnicornReg(values[0].getValue().getReg());
        long rawTarget = Integer.toUnsignedLong(context.getIntByReg(unicornReg));
        long codeTarget = rawTarget & ~1L;

        JSONObject row = baseRow(module, spec, offset);
        row.put("kind", "dispatch_runtime_target");
        row.put("callback_address", String.format("0x%x", callbackAddress));
        row.put("instruction", instruction.getMnemonic() +
                ((instruction.getOpStr() == null || instruction.getOpStr().isEmpty()) ? "" : " " + instruction.getOpStr()));
        row.put("target_register", instruction.getOpStr());
        row.put("target_raw", String.format("0x%08x", rawTarget));
        row.put("target_thumb", (rawTarget & 1L) != 0);
        row.put("target_code_address", String.format("0x%08x", codeTarget));
        putTargetIdentity(emulator, row, codeTarget);

        JSONObject args = new JSONObject(true);
        for (int i = 0; i < 4; i++)
            args.put("r" + i, String.format("0x%08x", Integer.toUnsignedLong(context.getIntArg(i))));
        row.put("arg_registers", args);

        // Receiver evidence is advisory and bounded. Failure to read it never removes
        // the already verified BLX target observation.
        DispatchReceiverProvenance.capture(emulator, row, spec, context, rawTarget);
        EvidenceProbe.emit(row);
    }

    private static JSONObject baseRow(Module module, JSONObject spec, long offset) {
        JSONObject row = new JSONObject(true);
        row.put("schema", 1);
        row.put("module", module.name);
        row.put("module_base", String.format("0x%x", module.base));
        row.put("source_id", spec.getString("source_id"));
        row.put("call_index", spec.getInteger("call_index"));
        row.put("call_address", spec.getString("call_address"));
        row.put("ghidra_call_site_address", spec.getString("ghidra_call_site_address"));
        row.put("ghidra_address_bias", spec.getString("ghidra_address_bias"));
        row.put("call_site_offset", String.format("0x%x", offset));
        row.put("slot_group_id", spec.getString("slot_group_id"));
        row.put("ordered_group_id", spec.getString("ordered_group_id"));
        row.put("ordered_position", spec.getInteger("ordered_position"));
        row.put("ordered_group_size", spec.getInteger("ordered_group_size"));
        return row;
    }

    private static void putTargetIdentity(AndroidEmulator emulator, JSONObject row, long codeTarget) {
        for (Module loaded : emulator.getMemory().getLoadedModules()) {
            if (codeTarget >= loaded.base && codeTarget < loaded.base + loaded.size) {
                long runtimeOffset = codeTarget - loaded.base;
                row.put("target_module", loaded.name);
                row.put("target_module_offset", String.format("0x%x", runtimeOffset));
                row.put("target_runtime_id", loaded.name + ":" + String.format("%08x", runtimeOffset));
                if ("libcocos2dcpp.so".equals(loaded.name)) {
                    long ghidraAddress = runtimeOffset + LIBCOCOS_GHIDRA_BIAS;
                    row.put("target_ghidra_address", String.format("0x%x", ghidraAddress));
                    row.put("target_id", loaded.name + ":" + String.format("%08x", ghidraAddress));
                } else {
                    row.put("target_ghidra_address", null);
                    row.put("target_id", loaded.name + ":" + String.format("%08x", runtimeOffset));
                }
                return;
            }
        }
        row.put("target_module", null);
        row.put("target_module_offset", null);
        row.put("target_runtime_id", "absolute:" + String.format("0x%08x", codeTarget));
        row.put("target_ghidra_address", null);
        row.put("target_id", "absolute:" + String.format("0x%08x", codeTarget));
    }

    private static long parseOffset(String value) {
        if (value == null || !value.matches("0x[0-9a-fA-F]+"))
            throw new IllegalArgumentException("Invalid hexadecimal offset: " + value);
        return Long.parseLong(value.substring(2), 16);
    }

    private DispatchTargetRuntimeTrace() {}
}
