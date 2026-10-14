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

// Evidence-only probe for CALLIND sites where the decompiler omitted the first
// explicit call argument. It recovers only a receiver varnode from the canonical
// virtual-dispatch data-flow shape:
//   CALLIND( LOAD( LOAD(receiver) + constant_slot ) )
// It never maps the indirect target address to a class/function and never mutates
// the program. For validation, sites with an explicit receiver are compared with
// the structurally recovered receiver. If the recovered receiver roots at a
// function parameter, direct caller argument types are also exported.
public class ExportIndirectCallTargetReceiverProvenance extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraTargetReceiverProvenance/1";
    private static final int MAX_TRANSPARENT_DEPTH = 6;
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

    private static class TypeInfo {
        final int pointerDepth;
        final DataType pointee;
        TypeInfo(int pointerDepth, DataType pointee) {
            this.pointerDepth = pointerDepth;
            this.pointee = pointee;
        }
    }

    private static class TargetBase {
        final Varnode base;
        final Long offset;
        final String pattern;
        TargetBase(Varnode base, Long offset, String pattern) {
            this.base = base;
            this.offset = offset;
            this.pattern = pattern;
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
        if (node.isConstant()) return "const:0x" + Long.toUnsignedString(node.getOffset(), 16) + ":" + node.getSize();
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

    private boolean isTransparent(PcodeOp op) {
        if (op == null) return false;
        int opcode = op.getOpcode();
        return opcode == PcodeOp.COPY || opcode == PcodeOp.CAST || opcode == PcodeOp.INDIRECT;
    }

    private Varnode stripTransparent(Varnode original, List<String> patterns) {
        Varnode cursor = original;
        HashSet<Varnode> seen = new HashSet<Varnode>();
        for (int depth = 0; cursor != null && depth < MAX_TRANSPARENT_DEPTH && seen.add(cursor); depth++) {
            PcodeOp def = cursor.getDef();
            if (!isTransparent(def) || def.getNumInputs() < 1) break;
            if (patterns != null) patterns.add(def.getMnemonic());
            cursor = def.getInput(0);
        }
        return cursor;
    }

    private boolean constantInput(PcodeOp op, int index) {
        return op != null && op.getNumInputs() > index && op.getInput(index) != null && op.getInput(index).isConstant();
    }

    private TargetBase decodeTargetAddress(Varnode original, List<String> patterns) {
        Varnode cursor = stripTransparent(original, patterns);
        PcodeOp def = cursor == null ? null : cursor.getDef();
        if (def == null) return new TargetBase(cursor, 0L, "DIRECT");
        int opcode = def.getOpcode();
        try {
            if (opcode == PcodeOp.PTRSUB && def.getNumInputs() >= 2 && constantInput(def, 1)) {
                if (patterns != null) patterns.add(def.getMnemonic());
                return new TargetBase(def.getInput(0), def.getInput(1).getOffset(), "PTRSUB");
            }
            if (opcode == PcodeOp.PTRADD && def.getNumInputs() >= 3 && constantInput(def, 1) && constantInput(def, 2)) {
                if (patterns != null) patterns.add(def.getMnemonic());
                long offset = Math.multiplyExact(def.getInput(1).getOffset(), def.getInput(2).getOffset());
                return new TargetBase(def.getInput(0), offset, "PTRADD");
            }
            if (opcode == PcodeOp.INT_ADD && def.getNumInputs() >= 2) {
                if (constantInput(def, 1)) {
                    if (patterns != null) patterns.add(def.getMnemonic());
                    return new TargetBase(def.getInput(0), def.getInput(1).getOffset(), "INT_ADD");
                }
                if (constantInput(def, 0)) {
                    if (patterns != null) patterns.add(def.getMnemonic());
                    return new TargetBase(def.getInput(1), def.getInput(0).getOffset(), "INT_ADD");
                }
            }
            if (opcode == PcodeOp.INT_SUB && def.getNumInputs() >= 2 && constantInput(def, 1)) {
                if (patterns != null) patterns.add(def.getMnemonic());
                return new TargetBase(def.getInput(0), -def.getInput(1).getOffset(), "INT_SUB");
            }
        } catch (ArithmeticException ignored) {
            return new TargetBase(null, null, "OFFSET_OVERFLOW");
        }
        return new TargetBase(null, null, def.getMnemonic());
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
            for (int i = 0; i < def.getNumInputs(); i++) collectTransparentTypes(def.getInput(i), depth + 1, seen, out);
        }
    }

    private void addSinglePointerPointee(DataType candidate, Set<String> names, Set<String> displays) {
        TypeInfo info = inspectType(candidate);
        if (info.pointerDepth != 1 || info.pointee == null) return;
        String name = info.pointee.getName();
        String display = info.pointee.getDisplayName();
        if (name != null && !name.isEmpty()) names.add(name);
        if (display != null && !display.isEmpty()) displays.add(display);
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
                cursor = def.getInput(0); continue;
            }
            if (opcode == PcodeOp.PTRSUB && def.getNumInputs() >= 2 && constantInput(def, 1)) {
                cursor = def.getInput(0); continue;
            }
            if (opcode == PcodeOp.PTRADD && def.getNumInputs() >= 3 && constantInput(def, 1) && constantInput(def, 2)) {
                cursor = def.getInput(0); continue;
            }
            if ((opcode == PcodeOp.INT_ADD || opcode == PcodeOp.INT_SUB) && def.getNumInputs() >= 2) {
                if (constantInput(def, 1)) { cursor = def.getInput(0); continue; }
                if (opcode == PcodeOp.INT_ADD && constantInput(def, 0)) { cursor = def.getInput(1); continue; }
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
        for (int i = 0; i < symbols.getNumParams(); i++) {
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
        } catch (Exception ignored) { return null; }
    }

    private Function resolveCallTarget(PcodeOpAST op) {
        Address target = callTargetAddress(op);
        if (target == null) return null;
        FunctionManager manager = currentProgram.getFunctionManager();
        Function function = manager.getFunctionAt(target);
        if (function != null) return function;
        long offset = target.getOffset();
        if ((offset & 1L) == 0L) return null;
        try { return manager.getFunctionAt(target.getAddressSpace().getAddress(offset & ~1L)); }
        catch (Exception ignored) { return null; }
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
        int matched = 0;
        if (paramIndex < 0) return 0;
        for (Function caller : directCallers(callee)) {
            if (monitor.isCancelled()) break;
            DecompileResults results = decompiler.decompileFunction(caller, 60, monitor);
            if (results == null || !results.decompileCompleted() || results.getHighFunction() == null) continue;
            Iterator<PcodeOpAST> operations = results.getHighFunction().getPcodeOps();
            while (operations.hasNext() && !monitor.isCancelled()) {
                PcodeOpAST op = operations.next();
                if (op.getOpcode() != PcodeOp.CALL) continue;
                Function target = resolveCallTarget(op);
                if (target == null || !target.getEntryPoint().equals(callee.getEntryPoint())) continue;
                int inputIndex = paramIndex + 1;
                if (op.getNumInputs() <= inputIndex) continue;
                matched++;
                callerEntries.add(cell(caller.getEntryPoint()));
                callSites.add(cell(op.getSeqnum().getTarget()));
                ArrayList<DataType> types = new ArrayList<DataType>();
                collectTransparentTypes(op.getInput(inputIndex), 0, new HashSet<Varnode>(), types);
                for (DataType type : types) addSinglePointerPointee(type, names, displays);
            }
        }
        return matched;
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
            String line; boolean first = true;
            while ((line = in.readLine()) != null) {
                if (line.trim().isEmpty()) continue;
                String[] cols = line.split("\\t", -1);
                if (first && cols.length >= 2 && "source_id".equals(cols[0])) { first = false; continue; }
                first = false;
                if (cols.length < 2) throw new IllegalArgumentException("Invalid plan row: " + line);
                int expected = cols.length >= 3 && !cols[2].isEmpty() ? Integer.parseInt(cols[2]) : -1;
                rows.add(new PlanRow(cols[0], cols[1], expected));
            }
        }
        return rows;
    }

    private void writeHeader(BufferedWriter out) throws Exception {
        out.write("format\tsource_id\tfunction_entry\tcall_index\tcall_address\tdirect_receiver_present\t" +
            "direct_receiver_token\tstructural_status\ttarget_pattern\ttarget_slot_offset\t" +
            "target_trace_patterns_json\trecovered_receiver_token\trecovered_receiver_type_display\t" +
            "recovered_receiver_pointer_depth\trecovered_pointee_type_display\tdirect_receiver_matches_structural\t" +
            "root_token\troot_type_display\troot_trace_patterns_json\troot_trace_nodes\tparameter_index\t" +
            "parameter_type_display\tdirect_caller_count\tmatched_caller_calls\tcaller_entries_json\t" +
            "caller_call_sites_json\tcaller_single_pointer_pointee_names_json\t" +
            "caller_single_pointer_pointee_displays_json");
        out.newLine();
    }

    private void writeCall(BufferedWriter out, DecompInterface decompiler, String sourceId, Function function,
            HighFunction high, int callIndex, PcodeOpAST call) throws Exception {
        SequenceNumber seq = call.getSeqnum();
        Varnode directReceiver = call.getNumInputs() >= 2 ? call.getInput(1) : null;
        ArrayList<String> targetPatterns = new ArrayList<String>();
        Varnode target = stripTransparent(call.getInput(0), targetPatterns);
        PcodeOp targetDef = target == null ? null : target.getDef();
        String structuralStatus = "target_not_load";
        String targetPattern = "-";
        Long targetOffset = null;
        Varnode recovered = null;

        if (targetDef != null && targetDef.getOpcode() == PcodeOp.LOAD && targetDef.getNumInputs() >= 2) {
            targetPatterns.add("LOAD_TARGET");
            TargetBase decoded = decodeTargetAddress(targetDef.getInput(1), targetPatterns);
            targetPattern = decoded.pattern;
            targetOffset = decoded.offset;
            Varnode vtable = stripTransparent(decoded.base, targetPatterns);
            PcodeOp vtableDef = vtable == null ? null : vtable.getDef();
            if (decoded.base == null) {
                structuralStatus = "target_address_not_constant_form";
            } else if (vtableDef == null || vtableDef.getOpcode() != PcodeOp.LOAD || vtableDef.getNumInputs() < 2) {
                structuralStatus = "vtable_not_load";
            } else {
                targetPatterns.add("LOAD_VTABLE");
                recovered = stripTransparent(vtableDef.getInput(1), targetPatterns);
                structuralStatus = recovered == null ? "receiver_missing" : "canonical_receiver";
            }
        }

        DataType recoveredType = highType(recovered);
        TypeInfo recoveredInfo = inspectType(recoveredType);
        String recoveredPointee = recoveredInfo.pointee == null ? "-" : recoveredInfo.pointee.getDisplayName();

        String match = "-";
        if (directReceiver != null && recovered != null) {
            Varnode directBase = stripTransparent(directReceiver, null);
            match = directBase != null && directBase.equals(recovered) ? "1" : "0";
        }

        RootTrace root = traceRoot(recovered);
        DataType rootType = highType(root.root);
        int paramIndex = parameterIndex(high, root.root);
        LinkedHashSet<String> callerNames = new LinkedHashSet<String>();
        LinkedHashSet<String> callerDisplays = new LinkedHashSet<String>();
        LinkedHashSet<String> callerEntries = new LinkedHashSet<String>();
        LinkedHashSet<String> callSites = new LinkedHashSet<String>();
        int directCallerCount = paramIndex >= 0 ? directCallers(function).size() : 0;
        int matchedCalls = collectCallerEvidence(decompiler, function, paramIndex, callerNames, callerDisplays, callerEntries, callSites);

        out.write(FORMAT + "\t" + cell(sourceId) + "\t" + cell(function.getEntryPoint()) + "\t" + callIndex + "\t" +
            cell(seq.getTarget()) + "\t" + (directReceiver == null ? 0 : 1) + "\t" + nodeToken(directReceiver) + "\t" +
            structuralStatus + "\t" + cell(targetPattern) + "\t" +
            (targetOffset == null ? "-" : "0x" + Long.toHexString(targetOffset)) + "\t" + jsonArray(targetPatterns) + "\t" +
            nodeToken(recovered) + "\t" + cell(recoveredType == null ? null : recoveredType.getDisplayName()) + "\t" +
            recoveredInfo.pointerDepth + "\t" + cell(recoveredPointee) + "\t" + match + "\t" + nodeToken(root.root) + "\t" +
            cell(rootType == null ? null : rootType.getDisplayName()) + "\t" + jsonArray(root.patterns) + "\t" + root.nodes + "\t" +
            paramIndex + "\t" + parameterTypeDisplay(high, paramIndex) + "\t" + directCallerCount + "\t" + matchedCalls + "\t" +
            jsonArray(callerEntries) + "\t" + jsonArray(callSites) + "\t" + jsonArray(callerNames) + "\t" + jsonArray(callerDisplays));
        out.newLine();
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) throw new IllegalArgumentException("Usage: ExportIndirectCallTargetReceiverProvenance.java <plan.tsv> <output.tsv>");
        File planPath = new File(args[0]);
        File outputPath = new File(args[1]);
        File parent = outputPath.getParentFile(); if (parent != null) parent.mkdirs();
        List<PlanRow> plan = readPlan(planPath);
        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException("Could not open current program in decompiler");
        int calls = 0, canonical = 0, missingDirectCanonical = 0, validationMatches = 0, validationMismatches = 0, expectedMismatches = 0;
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(outputPath), StandardCharsets.UTF_8))) {
            writeHeader(out);
            for (PlanRow item : plan) {
                if (monitor.isCancelled()) break;
                Function function = resolveFunction(item.sourceEntry);
                DecompileResults results = decompiler.decompileFunction(function, 60, monitor);
                if (results == null || !results.decompileCompleted() || results.getHighFunction() == null) continue;
                HighFunction high = results.getHighFunction();
                Iterator<PcodeOpAST> operations = high.getPcodeOps();
                int callIndex = 0;
                while (operations.hasNext() && !monitor.isCancelled()) {
                    PcodeOpAST op = operations.next();
                    if (op.getOpcode() != PcodeOp.CALLIND || op.getNumInputs() < 1) continue;
                    // Compute lightweight counters before writing the full row.
                    ArrayList<String> tmp = new ArrayList<String>();
                    Varnode target = stripTransparent(op.getInput(0), tmp);
                    PcodeOp targetDef = target == null ? null : target.getDef();
                    Varnode recovered = null;
                    if (targetDef != null && targetDef.getOpcode() == PcodeOp.LOAD && targetDef.getNumInputs() >= 2) {
                        TargetBase decoded = decodeTargetAddress(targetDef.getInput(1), null);
                        Varnode vtable = stripTransparent(decoded.base, null);
                        PcodeOp vtableDef = vtable == null ? null : vtable.getDef();
                        if (vtableDef != null && vtableDef.getOpcode() == PcodeOp.LOAD && vtableDef.getNumInputs() >= 2)
                            recovered = stripTransparent(vtableDef.getInput(1), null);
                    }
                    if (recovered != null) {
                        canonical++;
                        Varnode direct = op.getNumInputs() >= 2 ? stripTransparent(op.getInput(1), null) : null;
                        if (direct == null) missingDirectCanonical++;
                        else if (direct.equals(recovered)) validationMatches++;
                        else validationMismatches++;
                    }
                    writeCall(out, decompiler, item.sourceId, function, high, callIndex, op);
                    callIndex++; calls++;
                }
                if (item.expectedCalls >= 0 && item.expectedCalls != callIndex) expectedMismatches++;
            }
        } finally { decompiler.dispose(); }
        println("TARGET_RECEIVER_FORMAT=" + FORMAT);
        println("TARGET_RECEIVER_CALLS=" + calls);
        println("TARGET_RECEIVER_CANONICAL=" + canonical);
        println("TARGET_RECEIVER_MISSING_DIRECT_CANONICAL=" + missingDirectCanonical);
        println("TARGET_RECEIVER_VALIDATION_MATCHES=" + validationMatches);
        println("TARGET_RECEIVER_VALIDATION_MISMATCHES=" + validationMismatches);
        println("TARGET_RECEIVER_EXPECTED_CALL_MISMATCHES=" + expectedMismatches);
        println("TARGET_RECEIVER_OUTPUT=" + outputPath.getAbsolutePath());
        println("TARGET_RECEIVER_AFFECTS_ROUTING=no");
    }
}
