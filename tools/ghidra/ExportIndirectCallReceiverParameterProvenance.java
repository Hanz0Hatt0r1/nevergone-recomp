import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.Pointer;
import ghidra.program.model.data.TypeDef;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.HighSymbol;
import ghidra.program.model.pcode.HighVariable;
import ghidra.program.model.pcode.LocalSymbolMap;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.PcodeOpAST;
import ghidra.program.model.pcode.SequenceNumber;
import ghidra.program.model.pcode.Varnode;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

// Evidence-only probe for unresolved CALLIND receivers whose local type was lost.
// It follows only bounded, value-preserving/address-forming P-code backwards from
// a LOAD address to classify the root HighVariable. If that root is a function
// parameter, it inspects direct caller arguments at the same parameter index and
// exports their High-P-code types. It never infers from the indirect call target,
// never mutates the program, and never changes routing.
public class ExportIndirectCallReceiverParameterProvenance extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraReceiverParameterProvenance/1";
    private static final int MAX_ROOT_DEPTH = 8;
    private static final int MAX_TYPE_FLOW_DEPTH = 4;
    private static final int MAX_TYPE_FLOW_NODES = 16;
    private static final int MAX_CALLERS_PER_FUNCTION = 128;

    private static class PlanRow {
        final String sourceId;
        final String sourceEntry;
        final int expectedCalls;

        PlanRow(String sourceId, String sourceEntry, int expectedCalls) {
            this.sourceId = sourceId;
            this.sourceEntry = sourceEntry;
            this.expectedCalls = expectedCalls;
        }
    }

    private static class RootTrace {
        final Varnode root;
        final List<String> patterns;
        final int nodes;

        RootTrace(Varnode root, List<String> patterns, int nodes) {
            this.root = root;
            this.patterns = patterns;
            this.nodes = nodes;
        }
    }

    private static class TypeInfo {
        final int pointerDepth;
        final DataType pointee;

        TypeInfo(int pointerDepth, DataType pointee) {
            this.pointerDepth = pointerDepth;
            this.pointee = pointee;
        }
    }

    private String cell(Object value) {
        if (value == null) return "-";
        return String.valueOf(value).replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private String jsonString(String value) {
        if (value == null) return "null";
        StringBuilder out = new StringBuilder("\"");
        for (int i = 0; i < value.length(); i++) {
            char c = value.charAt(i);
            if (c == '\\' || c == '\"') out.append('\\').append(c);
            else if (c == '\n') out.append("\\n");
            else if (c == '\r') out.append("\\r");
            else if (c == '\t') out.append("\\t");
            else if (c < 0x20) out.append(String.format("\\u%04x", (int)c));
            else out.append(c);
        }
        return out.append('\"').toString();
    }

    private String jsonArray(Iterable<String> values) {
        StringBuilder out = new StringBuilder("[");
        boolean first = true;
        for (String value : values) {
            if (!first) out.append(',');
            first = false;
            out.append(jsonString(value));
        }
        return out.append(']').toString();
    }

    private String nodeToken(Varnode node) {
        if (node == null) return "-";
        if (node.isConstant()) {
            return "const:0x" + Long.toUnsignedString(node.getOffset(), 16) + ":" + node.getSize();
        }
        PcodeOp def = node.getDef();
        if (def != null) {
            SequenceNumber seq = def.getSeqnum();
            return "def:" + def.getMnemonic() + "@" + cell(seq.getTarget()) + ":" + seq.getTime() + ":" + node.getSize();
        }
        String kind = node.isRegister() ? "register" : node.isUnique() ? "unique" : node.isAddress() ? "address" : "input";
        return kind + ":" + cell(node.getAddress()) + ":" + node.getSize();
    }

    private DataType unwrapTypedef(DataType original) {
        DataType cursor = original;
        int guard = 0;
        while (cursor instanceof TypeDef && guard++ < 8) {
            DataType next = ((TypeDef) cursor).getBaseDataType();
            if (next == null || next == cursor) break;
            cursor = next;
        }
        return cursor;
    }

    private TypeInfo inspectType(DataType original) {
        DataType cursor = unwrapTypedef(original);
        int depth = 0;
        DataType pointee = null;
        while (cursor instanceof Pointer) {
            depth++;
            DataType next = unwrapTypedef(((Pointer) cursor).getDataType());
            if (depth == 1) pointee = next;
            if (next == null || next == cursor) break;
            cursor = next;
        }
        return new TypeInfo(depth, pointee);
    }

    private DataType highType(Varnode node) {
        HighVariable high = node == null ? null : node.getHigh();
        return high == null ? null : high.getDataType();
    }

    private void addSinglePointerPointee(DataType candidate, Set<String> names, Set<String> displays) {
        TypeInfo info = inspectType(candidate);
        if (info.pointerDepth != 1 || info.pointee == null) return;
        String name = info.pointee.getName();
        String display = info.pointee.getDisplayName();
        if (name != null && !name.isEmpty()) names.add(name);
        if (display != null && !display.isEmpty()) displays.add(display);
    }

    private void collectTransparentTypes(Varnode node, int depth, Set<Varnode> seen, List<DataType> out) {
        if (node == null || depth > MAX_TYPE_FLOW_DEPTH || seen.size() >= MAX_TYPE_FLOW_NODES || !seen.add(node)) return;
        DataType type = highType(node);
        if (type != null) out.add(type);
        PcodeOp def = node.getDef();
        if (def == null || depth == MAX_TYPE_FLOW_DEPTH) return;
        int opcode = def.getOpcode();
        if (opcode == PcodeOp.COPY || opcode == PcodeOp.CAST || opcode == PcodeOp.INDIRECT) {
            if (def.getNumInputs() >= 1) collectTransparentTypes(def.getInput(0), depth + 1, seen, out);
        } else if (opcode == PcodeOp.MULTIEQUAL) {
            for (int i = 0; i < def.getNumInputs(); i++) {
                collectTransparentTypes(def.getInput(i), depth + 1, seen, out);
            }
        }
    }

    private boolean constantInput(PcodeOp def, int index) {
        return def.getNumInputs() > index && def.getInput(index) != null && def.getInput(index).isConstant();
    }

    private RootTrace traceRoot(Varnode original) {
        Varnode cursor = original;
        ArrayList<String> patterns = new ArrayList<String>();
        HashSet<Varnode> seen = new HashSet<Varnode>();
        int nodes = 0;
        for (int depth = 0; cursor != null && depth < MAX_ROOT_DEPTH && seen.add(cursor); depth++) {
            nodes++;
            PcodeOp def = cursor.getDef();
            if (def == null) break;
            int opcode = def.getOpcode();
            patterns.add(def.getMnemonic());
            if ((opcode == PcodeOp.COPY || opcode == PcodeOp.CAST || opcode == PcodeOp.INDIRECT) && def.getNumInputs() >= 1) {
                cursor = def.getInput(0);
                continue;
            }
            if (opcode == PcodeOp.PTRSUB && def.getNumInputs() >= 2 && constantInput(def, 1)) {
                cursor = def.getInput(0);
                continue;
            }
            if (opcode == PcodeOp.PTRADD && def.getNumInputs() >= 3 && constantInput(def, 1) && constantInput(def, 2)) {
                cursor = def.getInput(0);
                continue;
            }
            if ((opcode == PcodeOp.INT_ADD || opcode == PcodeOp.INT_SUB) && def.getNumInputs() >= 2) {
                if (constantInput(def, 1)) {
                    cursor = def.getInput(0);
                    continue;
                }
                if (opcode == PcodeOp.INT_ADD && constantInput(def, 0)) {
                    cursor = def.getInput(1);
                    continue;
                }
            }
            break;
        }
        return new RootTrace(cursor, patterns, nodes);
    }

    private int parameterIndex(HighFunction high, Varnode root) {
        if (high == null || root == null || root.getHigh() == null) return -1;
        HighVariable rootHigh = root.getHigh();
        LocalSymbolMap symbols = high.getLocalSymbolMap();
        if (symbols == null) return -1;
        int count = symbols.getNumParams();
        for (int i = 0; i < count; i++) {
            HighSymbol symbol = symbols.getParamSymbol(i);
            if (symbol == null) continue;
            HighVariable paramHigh = symbol.getHighVariable();
            if (paramHigh == rootHigh) return i;
            if (paramHigh != null && paramHigh.getRepresentative() != null && rootHigh.getRepresentative() != null &&
                    paramHigh.getRepresentative().equals(rootHigh.getRepresentative())) return i;
        }
        return -1;
    }

    private String parameterTypeDisplay(HighFunction high, int index) {
        if (high == null || index < 0) return "-";
        LocalSymbolMap symbols = high.getLocalSymbolMap();
        if (symbols == null || index >= symbols.getNumParams()) return "-";
        HighSymbol symbol = symbols.getParamSymbol(index);
        if (symbol == null || symbol.getHighVariable() == null || symbol.getHighVariable().getDataType() == null) return "-";
        return cell(symbol.getHighVariable().getDataType().getDisplayName());
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
        FunctionManager manager = currentProgram.getFunctionManager();
        Function function = manager.getFunctionAt(target);
        if (function != null) return function;
        long offset = target.getOffset();
        if ((offset & 1L) == 0L) return null;
        try {
            Address normalized = target.getAddressSpace().getAddress(offset & ~1L);
            return manager.getFunctionAt(normalized);
        } catch (Exception ignored) {
            return null;
        }
    }

    private Set<Function> directCallers(Function callee) {
        LinkedHashSet<Function> callers = new LinkedHashSet<Function>();
        ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(callee.getEntryPoint());
        FunctionManager manager = currentProgram.getFunctionManager();
        while (refs.hasNext() && callers.size() < MAX_CALLERS_PER_FUNCTION) {
            Reference ref = refs.next();
            if (ref == null || ref.getFromAddress() == null) continue;
            Function caller = manager.getFunctionContaining(ref.getFromAddress());
            if (caller != null && !caller.equals(callee)) callers.add(caller);
        }
        return callers;
    }

    private int collectCallerEvidence(DecompInterface decompiler, Function callee, int paramIndex,
            Set<String> names, Set<String> displays, Set<String> callerEntries, Set<String> callSites) {
        int matchedCalls = 0;
        if (paramIndex < 0) return matchedCalls;
        for (Function caller : directCallers(callee)) {
            if (monitor.isCancelled()) break;
            DecompileResults results = decompiler.decompileFunction(caller, 60, monitor);
            if (results == null || !results.decompileCompleted() || results.getHighFunction() == null) continue;
            HighFunction high = results.getHighFunction();
            Iterator<PcodeOpAST> operations = high.getPcodeOps();
            while (operations.hasNext() && !monitor.isCancelled()) {
                PcodeOpAST op = operations.next();
                if (op.getOpcode() != PcodeOp.CALL) continue;
                Function target = resolveCallTarget(op);
                if (target == null || !target.getEntryPoint().equals(callee.getEntryPoint())) continue;
                int inputIndex = paramIndex + 1;
                if (op.getNumInputs() <= inputIndex) continue;
                matchedCalls++;
                callerEntries.add(cell(caller.getEntryPoint()));
                callSites.add(cell(op.getSeqnum().getTarget()));
                Varnode actual = op.getInput(inputIndex);
                ArrayList<DataType> types = new ArrayList<DataType>();
                collectTransparentTypes(actual, 0, new HashSet<Varnode>(), types);
                for (DataType type : types) addSinglePointerPointee(type, names, displays);
            }
        }
        return matchedCalls;
    }

    private Function resolveFunction(String token) throws Exception {
        Address address = toAddr(token);
        Function function = currentProgram.getFunctionManager().getFunctionAt(address);
        if (function == null) function = currentProgram.getFunctionManager().getFunctionContaining(address);
        if (function == null) throw new IllegalArgumentException("Function not found: " + token);
        return function;
    }

    private List<PlanRow> readPlan(File path) throws Exception {
        ArrayList<PlanRow> rows = new ArrayList<PlanRow>();
        try (BufferedReader in = new BufferedReader(new InputStreamReader(new FileInputStream(path), StandardCharsets.UTF_8))) {
            String line;
            boolean first = true;
            while ((line = in.readLine()) != null) {
                if (line.trim().isEmpty()) continue;
                String[] cols = line.split("\\t", -1);
                if (first && cols.length >= 2 && "source_id".equals(cols[0])) {
                    first = false;
                    continue;
                }
                first = false;
                if (cols.length < 2) throw new IllegalArgumentException("Invalid plan row: " + line);
                int expectedCalls = cols.length >= 3 && !cols[2].isEmpty() ? Integer.parseInt(cols[2]) : -1;
                rows.add(new PlanRow(cols[0], cols[1], expectedCalls));
            }
        }
        return rows;
    }

    private void writeHeader(BufferedWriter out) throws Exception {
        out.write("format\tsource_id\tfunction_entry\tcall_index\tcall_address\treceiver_is_load\t" +
            "root_token\troot_type_display\troot_trace_patterns_json\troot_trace_nodes\t" +
            "parameter_index\tparameter_type_display\tdirect_caller_count\tmatched_caller_calls\t" +
            "caller_entries_json\tcaller_call_sites_json\tcaller_single_pointer_pointee_names_json\t" +
            "caller_single_pointer_pointee_displays_json\tstatus");
        out.newLine();
    }

    private void writeCall(BufferedWriter out, DecompInterface decompiler, String sourceId, Function function,
            HighFunction high, int callIndex, PcodeOpAST call) throws Exception {
        SequenceNumber seq = call.getSeqnum();
        Varnode receiver = call.getNumInputs() >= 2 ? call.getInput(1) : null;
        PcodeOp receiverDef = receiver == null ? null : receiver.getDef();
        boolean isLoad = receiverDef != null && receiverDef.getOpcode() == PcodeOp.LOAD && receiverDef.getNumInputs() >= 2;
        Varnode address = isLoad ? receiverDef.getInput(1) : null;
        RootTrace trace = isLoad ? traceRoot(address) : new RootTrace(null, new ArrayList<String>(), 0);
        int paramIndex = isLoad ? parameterIndex(high, trace.root) : -1;

        LinkedHashSet<String> names = new LinkedHashSet<String>();
        LinkedHashSet<String> displays = new LinkedHashSet<String>();
        LinkedHashSet<String> callerEntries = new LinkedHashSet<String>();
        LinkedHashSet<String> callSites = new LinkedHashSet<String>();
        int callerCount = paramIndex >= 0 ? directCallers(function).size() : 0;
        int matchedCalls = paramIndex >= 0 ? collectCallerEvidence(
            decompiler, function, paramIndex, names, displays, callerEntries, callSites) : 0;

        String status;
        if (!isLoad) status = "not_load";
        else if (paramIndex < 0) status = "root_not_parameter";
        else if (matchedCalls == 0) status = "parameter_no_direct_caller_calls";
        else if (names.isEmpty() && displays.isEmpty()) status = "caller_calls_no_single_pointer_type";
        else status = "caller_type_candidate";

        out.write(FORMAT + "\t" + cell(sourceId) + "\t" + cell(function.getEntryPoint()) + "\t" +
            callIndex + "\t" + cell(seq.getTarget()) + "\t" + (isLoad ? 1 : 0) + "\t" +
            nodeToken(trace.root) + "\t" + cell(highType(trace.root) == null ? null : highType(trace.root).getDisplayName()) + "\t" +
            jsonArray(trace.patterns) + "\t" + trace.nodes + "\t" +
            (paramIndex >= 0 ? Integer.toString(paramIndex) : "-") + "\t" + parameterTypeDisplay(high, paramIndex) + "\t" +
            callerCount + "\t" + matchedCalls + "\t" + jsonArray(callerEntries) + "\t" + jsonArray(callSites) + "\t" +
            jsonArray(names) + "\t" + jsonArray(displays) + "\t" + status);
        out.newLine();
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException(
                "Usage: ExportIndirectCallReceiverParameterProvenance.java <plan.tsv> <output.tsv>");
        }
        File planPath = new File(args[0]);
        File outputPath = new File(args[1]);
        File parent = outputPath.getParentFile();
        if (parent != null) parent.mkdirs();
        List<PlanRow> plan = readPlan(planPath);

        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException("Could not open current program");

        int functionsOk = 0;
        int functionsFailed = 0;
        int calls = 0;
        int expectedMismatches = 0;
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(outputPath), StandardCharsets.UTF_8))) {
            writeHeader(out);
            for (PlanRow item : plan) {
                if (monitor.isCancelled()) break;
                Function function;
                try {
                    function = resolveFunction(item.sourceEntry);
                } catch (Exception error) {
                    printerr("PARAM_PROVENANCE_FUNCTION_NOT_FOUND source=" + item.sourceId + " entry=" + item.sourceEntry);
                    functionsFailed++;
                    continue;
                }
                DecompileResults results = decompiler.decompileFunction(function, 60, monitor);
                if (results == null || !results.decompileCompleted() || results.getHighFunction() == null) {
                    String message = results == null ? "no result" : results.getErrorMessage();
                    printerr("PARAM_PROVENANCE_DECOMPILE_FAILED source=" + item.sourceId + " message=" + cell(message));
                    functionsFailed++;
                    continue;
                }
                HighFunction high = results.getHighFunction();
                Iterator<PcodeOpAST> operations = high.getPcodeOps();
                int callIndex = 0;
                while (operations.hasNext() && !monitor.isCancelled()) {
                    PcodeOpAST op = operations.next();
                    if (op.getOpcode() != PcodeOp.CALLIND || op.getNumInputs() < 1) continue;
                    writeCall(out, decompiler, item.sourceId, function, high, callIndex, op);
                    callIndex++;
                    calls++;
                }
                if (item.expectedCalls >= 0 && item.expectedCalls != callIndex) {
                    expectedMismatches++;
                    printerr("PARAM_PROVENANCE_CALL_COUNT_MISMATCH source=" + item.sourceId +
                        " expected=" + item.expectedCalls + " actual=" + callIndex);
                }
                functionsOk++;
            }
        } finally {
            decompiler.dispose();
        }

        println("RECEIVER_PARAMETER_PROVENANCE_FORMAT=" + FORMAT);
        println("RECEIVER_PARAMETER_PROVENANCE_PLAN_FUNCTIONS=" + plan.size());
        println("RECEIVER_PARAMETER_PROVENANCE_FUNCTIONS_OK=" + functionsOk);
        println("RECEIVER_PARAMETER_PROVENANCE_FUNCTIONS_FAILED=" + functionsFailed);
        println("RECEIVER_PARAMETER_PROVENANCE_CALLS=" + calls);
        println("RECEIVER_PARAMETER_PROVENANCE_EXPECTED_CALL_MISMATCHES=" + expectedMismatches);
        println("RECEIVER_PARAMETER_PROVENANCE_OUTPUT=" + outputPath.getAbsolutePath());
    }
}
