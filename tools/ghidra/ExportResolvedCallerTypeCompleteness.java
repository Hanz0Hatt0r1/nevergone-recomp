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
import ghidra.program.model.pcode.HighVariable;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.PcodeOpAST;
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

// Evidence-only verifier for receiver classes recovered from direct-caller argument types.
// Every direct caller function must decompile, every matched call site must expose a usable
// one-level pointer type, and no target inference is performed here.
public class ExportResolvedCallerTypeCompleteness extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraResolvedCallerTypeCompleteness/1";
    private static final int MAX_TYPE_FLOW_DEPTH = 4;
    private static final int MAX_TYPE_FLOW_NODES = 16;
    private static final int MAX_CALLERS_PER_FUNCTION = 128;

    private static class PlanRow {
        String sourceId;
        String sourceEntry;
        int callIndex;
        int parameterIndex;
        String evidenceSource;
        String inferredClassName;
        String dispatchSlotOffset;
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

    private void addSinglePointerPointee(DataType candidate, Set<String> names, Set<String> displays) {
        TypeInfo info = inspectType(candidate);
        if (info.pointerDepth != 1 || info.pointee == null) return;
        String name = info.pointee.getName();
        String display = info.pointee.getDisplayName();
        if (name != null && !name.isEmpty()) names.add(name);
        if (display != null && !display.isEmpty()) displays.add(display);
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
            return manager.getFunctionAt(target.getAddressSpace().getAddress(offset & ~1L));
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
                if (first && cols.length >= 7 && "source_id".equals(cols[0])) {
                    first = false;
                    continue;
                }
                first = false;
                if (cols.length < 7) throw new IllegalArgumentException("Invalid plan row: " + line);
                PlanRow row = new PlanRow();
                row.sourceId = cols[0];
                row.sourceEntry = cols[1];
                row.callIndex = Integer.parseInt(cols[2]);
                row.parameterIndex = Integer.parseInt(cols[3]);
                row.evidenceSource = cols[4];
                row.inferredClassName = cols[5];
                row.dispatchSlotOffset = cols[6];
                rows.add(row);
            }
        }
        return rows;
    }

    private void writeHeader(BufferedWriter out) throws Exception {
        out.write("format\tsource_id\tfunction_entry\tcall_index\tparameter_index\tevidence_source\t" +
            "inferred_class_name\tdispatch_slot_offset\tdirect_caller_functions\tcaller_functions_decompiled\t" +
            "caller_functions_failed\tcallers_with_matched_calls\tmatched_caller_calls\ttyped_caller_calls\t" +
            "untyped_caller_calls\tcaller_entries_json\tcall_sites_json\ttyped_call_sites_json\t" +
            "untyped_call_sites_json\tcaller_single_pointer_pointee_names_json\t" +
            "caller_single_pointer_pointee_displays_json\tstatus");
        out.newLine();
    }

    private void writeRow(BufferedWriter out, DecompInterface decompiler, PlanRow plan) throws Exception {
        Function callee = resolveFunction(plan.sourceEntry);
        Set<Function> callers = directCallers(callee);
        int decompiled = 0;
        int failed = 0;
        int callersWithMatched = 0;
        int matched = 0;
        int typed = 0;
        int untyped = 0;
        LinkedHashSet<String> callerEntries = new LinkedHashSet<String>();
        LinkedHashSet<String> callSites = new LinkedHashSet<String>();
        LinkedHashSet<String> typedCallSites = new LinkedHashSet<String>();
        LinkedHashSet<String> untypedCallSites = new LinkedHashSet<String>();
        LinkedHashSet<String> names = new LinkedHashSet<String>();
        LinkedHashSet<String> displays = new LinkedHashSet<String>();

        for (Function caller : callers) {
            if (monitor.isCancelled()) break;
            DecompileResults results = decompiler.decompileFunction(caller, 60, monitor);
            if (results == null || !results.decompileCompleted() || results.getHighFunction() == null) {
                failed++;
                continue;
            }
            decompiled++;
            HighFunction high = results.getHighFunction();
            boolean callerMatched = false;
            Iterator<PcodeOpAST> operations = high.getPcodeOps();
            while (operations.hasNext() && !monitor.isCancelled()) {
                PcodeOpAST op = operations.next();
                if (op.getOpcode() != PcodeOp.CALL) continue;
                Function target = resolveCallTarget(op);
                if (target == null || !target.getEntryPoint().equals(callee.getEntryPoint())) continue;
                int inputIndex = plan.parameterIndex + 1;
                if (op.getNumInputs() <= inputIndex) continue;
                callerMatched = true;
                matched++;
                String callerEntry = cell(caller.getEntryPoint());
                String callSite = cell(op.getSeqnum().getTarget());
                callerEntries.add(callerEntry);
                callSites.add(callSite);

                Varnode actual = op.getInput(inputIndex);
                ArrayList<DataType> types = new ArrayList<DataType>();
                collectTransparentTypes(actual, 0, new HashSet<Varnode>(), types);
                LinkedHashSet<String> localNames = new LinkedHashSet<String>();
                LinkedHashSet<String> localDisplays = new LinkedHashSet<String>();
                for (DataType type : types) addSinglePointerPointee(type, localNames, localDisplays);
                if (localNames.isEmpty() && localDisplays.isEmpty()) {
                    untyped++;
                    untypedCallSites.add(callSite);
                } else {
                    typed++;
                    typedCallSites.add(callSite);
                    names.addAll(localNames);
                    displays.addAll(localDisplays);
                }
            }
            if (callerMatched) callersWithMatched++;
        }

        String status;
        if (callers.isEmpty()) status = "no_direct_callers";
        else if (failed != 0) status = "caller_decompile_incomplete";
        else if (callersWithMatched != callers.size()) status = "caller_function_without_matched_call";
        else if (matched == 0) status = "no_matched_caller_calls";
        else if (untyped != 0 || typed != matched) status = "partial_or_untyped_caller_calls";
        else status = "complete_typed_callers";

        out.write(FORMAT + "\t" + cell(plan.sourceId) + "\t" + cell(callee.getEntryPoint()) + "\t" +
            plan.callIndex + "\t" + plan.parameterIndex + "\t" + cell(plan.evidenceSource) + "\t" +
            cell(plan.inferredClassName) + "\t" + cell(plan.dispatchSlotOffset) + "\t" + callers.size() + "\t" +
            decompiled + "\t" + failed + "\t" + callersWithMatched + "\t" + matched + "\t" + typed + "\t" +
            untyped + "\t" + jsonArray(callerEntries) + "\t" + jsonArray(callSites) + "\t" +
            jsonArray(typedCallSites) + "\t" + jsonArray(untypedCallSites) + "\t" + jsonArray(names) + "\t" +
            jsonArray(displays) + "\t" + status);
        out.newLine();
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException("Usage: ExportResolvedCallerTypeCompleteness.java <plan.tsv> <output.tsv>");
        }
        List<PlanRow> plan = readPlan(new File(args[0]));
        File output = new File(args[1]);
        File parent = output.getParentFile();
        if (parent != null) parent.mkdirs();

        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException("Could not open current program");
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(output), StandardCharsets.UTF_8))) {
            writeHeader(out);
            for (PlanRow row : plan) {
                if (monitor.isCancelled()) break;
                writeRow(out, decompiler, row);
            }
        } finally {
            decompiler.dispose();
        }
        println("RESOLVED_CALLER_COMPLETENESS_FORMAT=" + FORMAT);
        println("RESOLVED_CALLER_COMPLETENESS_ROWS=" + plan.size());
        println("RESOLVED_CALLER_COMPLETENESS_OUTPUT=" + output.getAbsolutePath());
        println("RESOLVED_CALLER_COMPLETENESS_AFFECTS_ROUTING=no");
    }
}
