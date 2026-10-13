import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.PcodeOpAST;
import ghidra.program.model.pcode.SequenceNumber;
import ghidra.program.model.pcode.Varnode;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

// Bulk clean-room high-P-code exporter intended for local ML dataset generation.
//
// The model_text field deliberately excludes:
//   * absolute function/instruction addresses;
//   * function and symbol names;
//   * assembly bytes/disassembly;
//   * decompiler C;
//   * strings/assets.
//
// source_entry/source_name are join keys only. Dataset builders must not copy
// them into model input. Large constants are bucketed so embedded code/data
// addresses do not become accidental identifiers. Small constants are retained
// because field offsets, loop bounds and flags are useful structural evidence.
public class ExportMlPcodeCorpus extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.MlPcodeCorpus/1";
    private static final long SMALL_CONST_MAX = 0x1000L;

    private String json(String value) {
        if (value == null) return "null";
        StringBuilder out = new StringBuilder();
        out.append('"');
        for (int i = 0; i < value.length(); i++) {
            char c = value.charAt(i);
            switch (c) {
                case '\\': out.append("\\\\"); break;
                case '"': out.append("\\\""); break;
                case '\n': out.append("\\n"); break;
                case '\r': out.append("\\r"); break;
                case '\t': out.append("\\t"); break;
                default:
                    if (c < 0x20) {
                        out.append(String.format("\\u%04x", (int)c));
                    } else {
                        out.append(c);
                    }
            }
        }
        out.append('"');
        return out.toString();
    }

    private String seqKey(SequenceNumber sequence) {
        return sequence.getTarget().toString() + ":" + sequence.getTime();
    }

    private String constantToken(Varnode node) {
        long value = node.getOffset();
        int size = node.getSize();
        if (value == 0L) return "const:zero:" + size;
        if (value == 1L) return "const:one:" + size;
        long mask = size >= 8 ? -1L : ((1L << (size * 8)) - 1L);
        if (value == mask) return "const:allones:" + size;
        if (Long.compareUnsigned(value, SMALL_CONST_MAX) <= 0) {
            return "const:small:0x" + Long.toUnsignedString(value, 16) + ":" + size;
        }
        return "const:large:" + size;
    }

    private String varnodeToken(Varnode node, Map<String, Integer> opIds) {
        if (node == null) return "none";
        if (node.isConstant()) return constantToken(node);

        PcodeOp definition = node.getDef();
        if (definition != null) {
            Integer id = opIds.get(seqKey(definition.getSeqnum()));
            if (id != null) return "v" + id + ":" + node.getSize();
            return "def:unknown:" + node.getSize();
        }
        if (node.isRegister()) return "reg:" + node.getSize();
        if (node.isUnique()) return "tmp:" + node.getSize();
        if (node.isAddress()) return "mem:" + node.getSize();
        return "input:" + node.getSize();
    }

    private Address callTargetAddress(PcodeOpAST op) {
        if (op.getOpcode() != PcodeOp.CALL || op.getNumInputs() < 1) return null;
        Varnode target = op.getInput(0);
        if (target == null) return null;
        if (!target.isConstant()) return target.getAddress();
        try {
            return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(target.getOffset());
        } catch (Exception ignored) {
            return null;
        }
    }

    private Function resolveCallTarget(PcodeOpAST op) {
        Address target = callTargetAddress(op);
        if (target == null) return null;
        Function function = currentProgram.getFunctionManager().getFunctionAt(target);
        if (function != null) return function;

        // ARM/Thumb pointers may carry bit 0.
        long offset = target.getOffset();
        if ((offset & 1L) == 0L) return null;
        try {
            Address normalized = target.getAddressSpace().getAddress(offset & ~1L);
            return currentProgram.getFunctionManager().getFunctionAt(normalized);
        } catch (Exception ignored) {
            return null;
        }
    }

    private String callKind(PcodeOpAST op) {
        if (op.getOpcode() == PcodeOp.CALLIND) return "call:indirect";
        if (op.getOpcode() != PcodeOp.CALL) return null;
        Function target = resolveCallTarget(op);
        if (target == null) return "call:unresolved";
        return target.isExternal() ? "call:external" : "call:internal";
    }

    private String buildModelText(Function function, HighFunction high) {
        List<PcodeOpAST> ops = new ArrayList<>();
        Map<String, Integer> opIds = new HashMap<>();
        Iterator<PcodeOpAST> iterator = high.getPcodeOps();
        while (iterator.hasNext() && !monitor.isCancelled()) {
            PcodeOpAST op = iterator.next();
            int id = ops.size();
            ops.add(op);
            opIds.put(seqKey(op.getSeqnum()), id);
        }

        StringBuilder text = new StringBuilder();
        text.append("LANG=").append(currentProgram.getLanguageID().toString()).append('\n');
        text.append("CALLCONV=").append(function.getCallingConventionName()).append('\n');
        text.append("PARAMS=").append(function.getParameterCount()).append('\n');
        text.append("BODY_ADDRS=").append(function.getBody().getNumAddresses()).append('\n');
        text.append("OPS=").append(ops.size()).append('\n');

        for (int id = 0; id < ops.size(); id++) {
            PcodeOpAST op = ops.get(id);
            text.append('o').append(id).append(' ').append(op.getMnemonic());
            String kind = callKind(op);
            if (kind != null) text.append(' ').append(kind);

            Varnode output = op.getOutput();
            if (output != null) {
                text.append(" out=").append(varnodeToken(output, opIds));
            }

            int start = (op.getOpcode() == PcodeOp.CALL || op.getOpcode() == PcodeOp.CALLIND) ? 1 : 0;
            for (int index = start; index < op.getNumInputs(); index++) {
                text.append(" in=").append(varnodeToken(op.getInput(index), opIds));
            }
            text.append('\n');
        }
        return text.toString();
    }

    private void writeSuccess(BufferedWriter out, Function function, String modelText) throws Exception {
        out.write("{" +
            "\"format\":" + json(FORMAT) + "," +
            "\"program\":" + json(currentProgram.getName()) + "," +
            "\"source_entry\":" + json(function.getEntryPoint().toString()) + "," +
            "\"source_name\":" + json(function.getName(true)) + "," +
            "\"external\":false," +
            "\"decompile_status\":\"ok\"," +
            "\"model_text\":" + json(modelText) +
            "}");
        out.newLine();
    }

    private void writeFailure(BufferedWriter out, Function function, String message) throws Exception {
        out.write("{" +
            "\"format\":" + json(FORMAT) + "," +
            "\"program\":" + json(currentProgram.getName()) + "," +
            "\"source_entry\":" + json(function.getEntryPoint().toString()) + "," +
            "\"source_name\":" + json(function.getName(true)) + "," +
            "\"external\":false," +
            "\"decompile_status\":\"failed\"," +
            "\"error\":" + json(message == null ? "unknown decompiler failure" : message) +
            "}");
        out.newLine();
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1 || args.length > 3) {
            throw new IllegalArgumentException(
                "Usage: ExportMlPcodeCorpus.java <output.jsonl> [max-functions] [name-contains]");
        }

        File output = new File(args[0]);
        File parent = output.getParentFile();
        if (parent != null) parent.mkdirs();
        int maxFunctions = args.length >= 2 ? Integer.parseInt(args[1]) : 0;
        String nameFilter = args.length >= 3 ? args[2] : "";

        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) {
            throw new IllegalStateException("Could not open current program in decompiler");
        }

        long visited = 0;
        long exported = 0;
        long failed = 0;
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(
                new FileOutputStream(output), StandardCharsets.UTF_8))) {
            FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
            while (functions.hasNext() && !monitor.isCancelled()) {
                Function function = functions.next();
                if (function.isExternal()) continue;
                if (!nameFilter.isEmpty() &&
                    !function.getName(true).contains(nameFilter) &&
                    !function.getName().contains(nameFilter)) {
                    continue;
                }
                if (maxFunctions > 0 && visited >= maxFunctions) break;
                visited++;

                DecompileResults results = decompiler.decompileFunction(function, 60, monitor);
                if (results == null || !results.decompileCompleted() || results.getHighFunction() == null) {
                    String message = results == null ? "no result" : results.getErrorMessage();
                    writeFailure(out, function, message);
                    failed++;
                    continue;
                }

                writeSuccess(out, function, buildModelText(function, results.getHighFunction()));
                exported++;
                if ((visited % 250) == 0) {
                    println("ML corpus progress: visited=" + visited + " exported=" + exported + " failed=" + failed);
                }
            }
        } finally {
            decompiler.dispose();
        }

        println("ML P-code corpus: visited=" + visited + " exported=" + exported +
            " failed=" + failed + " -> " + output);
    }
}
