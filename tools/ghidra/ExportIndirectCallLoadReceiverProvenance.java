import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.DataTypeComponent;
import ghidra.program.model.data.Pointer;
import ghidra.program.model.data.Structure;
import ghidra.program.model.data.TypeDef;
import ghidra.program.model.listing.Function;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.HighVariable;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.PcodeOpAST;
import ghidra.program.model.pcode.SequenceNumber;
import ghidra.program.model.pcode.Varnode;

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

// Evidence-only diagnostic for CALLIND receivers defined by LOAD. It never infers
// from the call target and never mutates the program. Version 2 recursively follows
// bounded address provenance through transparent casts/copies and constant address
// arithmetic. Candidate receiver classes are emitted only when the provenance itself
// supplies either T ** at the exact LOAD address or an exact typed Structure field T *.
public class ExportIndirectCallLoadReceiverProvenance extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraLoadReceiverProvenance/2";
    private static final int MAX_PROVENANCE_DEPTH = 8;
    private static final int MAX_PROVENANCE_NODES = 48;

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
        final DataType type;
        final int pointerDepth;
        final DataType pointee;

        TypeInfo(DataType type, int pointerDepth, DataType pointee) {
            this.type = type;
            this.pointerDepth = pointerDepth;
            this.pointee = pointee;
        }
    }

    private static class ImmediateAddress {
        final String pattern;
        final Varnode base;
        final Long byteOffset;

        ImmediateAddress(String pattern, Varnode base, Long byteOffset) {
            this.pattern = pattern;
            this.base = base;
            this.byteOffset = byteOffset;
        }
    }

    private static class ProvenanceEvidence {
        final LinkedHashSet<String> candidateNames = new LinkedHashSet<String>();
        final LinkedHashSet<String> candidateDisplays = new LinkedHashSet<String>();
        final LinkedHashSet<String> patterns = new LinkedHashSet<String>();
        final LinkedHashSet<String> trace = new LinkedHashSet<String>();
        final LinkedHashSet<String> typedFields = new LinkedHashSet<String>();
        int visitedNodes = 0;
        String firstBaseToken = null;
        String firstBaseTypeDisplay = null;
        Long firstByteOffset = null;
        String firstComponentFieldName = null;
        String firstComponentTypeDisplay = null;
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

    private String jsonArray(Set<String> values) {
        StringBuilder out = new StringBuilder("[");
        boolean first = true;
        for (String value : values) {
            if (!first) out.append(',');
            first = false;
            out.append(jsonString(value));
        }
        return out.append(']').toString();
    }

    private String storageKind(Varnode node) {
        if (node == null) return "missing";
        if (node.isConstant()) return "constant";
        if (node.isRegister()) return "register";
        if (node.isUnique()) return "unique";
        if (node.isAddress()) return "address";
        return "other";
    }

    private String nodeToken(Varnode node) {
        if (node == null) return "-";
        if (node.isConstant()) {
            return "const:0x" + Long.toUnsignedString(node.getOffset(), 16) + ":" + node.getSize();
        }
        PcodeOp def = node.getDef();
        if (def != null) {
            SequenceNumber seq = def.getSeqnum();
            return "def:" + def.getMnemonic() + "@" + cell(seq.getTarget()) + ":" +
                seq.getTime() + ":" + node.getSize();
        }
        return storageKind(node) + ":" + cell(node.getAddress()) + ":" + node.getSize();
    }

    private DataType unwrapTypedef(DataType type) {
        DataType cursor = type;
        int guard = 0;
        while (cursor instanceof TypeDef && guard++ < 8) {
            DataType next = ((TypeDef) cursor).getBaseDataType();
            if (next == null || next == cursor) break;
            cursor = next;
        }
        return cursor;
    }

    private TypeInfo inspectType(DataType original) {
        DataType type = unwrapTypedef(original);
        int pointerDepth = 0;
        DataType pointee = null;
        DataType cursor = type;
        while (cursor instanceof Pointer) {
            pointerDepth++;
            DataType next = unwrapTypedef(((Pointer) cursor).getDataType());
            if (pointerDepth == 1) pointee = next;
            if (next == null || next == cursor) break;
            cursor = next;
        }
        return new TypeInfo(type, pointerDepth, pointee);
    }

    private DataType highType(Varnode node) {
        HighVariable high = node == null ? null : node.getHigh();
        return high == null ? null : high.getDataType();
    }

    private DataType dereferenceOnce(DataType original) {
        DataType type = unwrapTypedef(original);
        if (!(type instanceof Pointer)) return null;
        return unwrapTypedef(((Pointer) type).getDataType());
    }

    private void addSinglePointerPointee(DataType candidate, Set<String> names, Set<String> displays) {
        TypeInfo info = inspectType(candidate);
        if (info.pointerDepth != 1 || info.pointee == null) return;
        String name = info.pointee.getName();
        String display = info.pointee.getDisplayName();
        if (name != null && !name.isEmpty()) names.add(name);
        if (display != null && !display.isEmpty()) displays.add(display);
    }

    private long signedConstant(Varnode node) {
        long value = node.getOffset();
        int bits = node.getSize() * 8;
        if (bits <= 0 || bits >= 64) return value;
        long mask = (1L << bits) - 1L;
        long sign = 1L << (bits - 1);
        value &= mask;
        if ((value & sign) != 0) value -= (1L << bits);
        return value;
    }

    private Long addOffset(Long base, long delta) {
        if (base == null) return null;
        try {
            return Math.addExact(base.longValue(), delta);
        } catch (ArithmeticException ignored) {
            return null;
        }
    }

    private Long multiply(long left, long right) {
        try {
            return Math.multiplyExact(left, right);
        } catch (ArithmeticException ignored) {
            return null;
        }
    }

    private ImmediateAddress decodeImmediateAddress(Varnode original) {
        Varnode address = original;
        int unwraps = 0;
        while (address != null && unwraps++ < 4) {
            PcodeOp def = address.getDef();
            if (def == null || def.getNumInputs() < 1) break;
            int opcode = def.getOpcode();
            if (opcode != PcodeOp.COPY && opcode != PcodeOp.CAST && opcode != PcodeOp.INDIRECT) break;
            address = def.getInput(0);
        }
        if (address == null) return new ImmediateAddress("none", null, null);
        PcodeOp def = address.getDef();
        if (def == null) return new ImmediateAddress("direct", address, 0L);
        if (def.getOpcode() == PcodeOp.PTRSUB && def.getNumInputs() >= 2 && def.getInput(1).isConstant()) {
            return new ImmediateAddress("PTRSUB", def.getInput(0), signedConstant(def.getInput(1)));
        }
        if (def.getOpcode() == PcodeOp.PTRADD && def.getNumInputs() >= 3 &&
                def.getInput(1).isConstant() && def.getInput(2).isConstant()) {
            Long delta = multiply(signedConstant(def.getInput(1)), signedConstant(def.getInput(2)));
            return new ImmediateAddress("PTRADD", def.getInput(0), delta);
        }
        if (def.getOpcode() == PcodeOp.INT_ADD && def.getNumInputs() >= 2) {
            Varnode a = def.getInput(0);
            Varnode b = def.getInput(1);
            if (a.isConstant() ^ b.isConstant()) {
                Varnode c = a.isConstant() ? a : b;
                Varnode base = a.isConstant() ? b : a;
                return new ImmediateAddress("INT_ADD", base, signedConstant(c));
            }
        }
        if (def.getOpcode() == PcodeOp.INT_SUB && def.getNumInputs() >= 2 && def.getInput(1).isConstant()) {
            long delta = signedConstant(def.getInput(1));
            if (delta != Long.MIN_VALUE) return new ImmediateAddress("INT_SUB", def.getInput(0), -delta);
        }
        return new ImmediateAddress(def.getMnemonic(), null, null);
    }

    private void inspectProvenanceNode(Varnode node, Long byteOffset, int depth,
            ProvenanceEvidence evidence) {
        DataType type = highType(node);
        String typeDisplay = type == null ? "-" : type.getDisplayName();
        evidence.trace.add(
            "depth=" + depth + " token=" + nodeToken(node) + " offset=" +
            (byteOffset == null ? "?" : "0x" + Long.toHexString(byteOffset.longValue())) +
            " type=" + cell(typeDisplay)
        );

        if (byteOffset != null && byteOffset.longValue() == 0L) {
            addSinglePointerPointee(dereferenceOnce(type), evidence.candidateNames, evidence.candidateDisplays);
        }

        TypeInfo info = inspectType(type);
        DataType pointee = unwrapTypedef(info.pointee);
        if (byteOffset == null || byteOffset.longValue() < 0 || byteOffset.longValue() > Integer.MAX_VALUE ||
                info.pointerDepth != 1 || !(pointee instanceof Structure)) {
            return;
        }

        Structure structure = (Structure) pointee;
        DataTypeComponent component = structure.getComponentAt(byteOffset.intValue());
        if (component == null || component.getOffset() != byteOffset.intValue()) return;

        DataType componentType = unwrapTypedef(component.getDataType());
        String fieldName = component.getFieldName();
        String componentDisplay = componentType == null ? null : componentType.getDisplayName();
        String structureDisplay = pointee.getDisplayName();
        evidence.typedFields.add(
            cell(structureDisplay) + "@0x" + Long.toHexString(byteOffset.longValue()) +
            " field=" + cell(fieldName) + " type=" + cell(componentDisplay)
        );
        addSinglePointerPointee(componentType, evidence.candidateNames, evidence.candidateDisplays);

        if (evidence.firstComponentTypeDisplay == null) {
            evidence.firstBaseToken = nodeToken(node);
            evidence.firstBaseTypeDisplay = typeDisplay;
            evidence.firstByteOffset = byteOffset;
            evidence.firstComponentFieldName = fieldName;
            evidence.firstComponentTypeDisplay = componentDisplay;
        }
    }

    private void traceAddressProvenance(Varnode node, Long byteOffset, int depth,
            Set<Varnode> seen, ProvenanceEvidence evidence) {
        if (node == null || depth > MAX_PROVENANCE_DEPTH || evidence.visitedNodes >= MAX_PROVENANCE_NODES ||
                !seen.add(node)) return;
        evidence.visitedNodes++;
        inspectProvenanceNode(node, byteOffset, depth, evidence);

        PcodeOp def = node.getDef();
        if (def == null || depth == MAX_PROVENANCE_DEPTH) return;
        int opcode = def.getOpcode();

        if (opcode == PcodeOp.COPY || opcode == PcodeOp.CAST || opcode == PcodeOp.INDIRECT) {
            evidence.patterns.add(def.getMnemonic());
            if (def.getNumInputs() >= 1)
                traceAddressProvenance(def.getInput(0), byteOffset, depth + 1, seen, evidence);
            return;
        }

        if (opcode == PcodeOp.PTRSUB && def.getNumInputs() >= 2 && def.getInput(1).isConstant()) {
            evidence.patterns.add("PTRSUB");
            traceAddressProvenance(def.getInput(0), addOffset(byteOffset, signedConstant(def.getInput(1))),
                depth + 1, seen, evidence);
            return;
        }

        if (opcode == PcodeOp.PTRADD && def.getNumInputs() >= 3 &&
                def.getInput(1).isConstant() && def.getInput(2).isConstant()) {
            evidence.patterns.add("PTRADD");
            Long delta = multiply(signedConstant(def.getInput(1)), signedConstant(def.getInput(2)));
            Long nextOffset = delta == null ? null : addOffset(byteOffset, delta.longValue());
            traceAddressProvenance(def.getInput(0), nextOffset, depth + 1, seen, evidence);
            return;
        }

        if (opcode == PcodeOp.INT_ADD && def.getNumInputs() >= 2) {
            evidence.patterns.add("INT_ADD");
            Varnode a = def.getInput(0);
            Varnode b = def.getInput(1);
            if (a.isConstant() ^ b.isConstant()) {
                Varnode c = a.isConstant() ? a : b;
                Varnode base = a.isConstant() ? b : a;
                traceAddressProvenance(base, addOffset(byteOffset, signedConstant(c)),
                    depth + 1, seen, evidence);
            }
            return;
        }

        if (opcode == PcodeOp.INT_SUB && def.getNumInputs() >= 2 && def.getInput(1).isConstant()) {
            evidence.patterns.add("INT_SUB");
            long delta = signedConstant(def.getInput(1));
            Long nextOffset = delta == Long.MIN_VALUE ? null : addOffset(byteOffset, -delta);
            traceAddressProvenance(def.getInput(0), nextOffset, depth + 1, seen, evidence);
            return;
        }

        if (opcode == PcodeOp.MULTIEQUAL) {
            evidence.patterns.add("MULTIEQUAL");
            for (int i = 0; i < def.getNumInputs(); i++) {
                traceAddressProvenance(def.getInput(i), byteOffset, depth + 1,
                    new HashSet<Varnode>(seen), evidence);
            }
        }
    }

    private Function resolveFunction(String token) throws Exception {
        Address address = toAddr(token);
        Function function = currentProgram.getFunctionManager().getFunctionAt(address);
        if (function == null) function = currentProgram.getFunctionManager().getFunctionContaining(address);
        if (function == null) throw new IllegalArgumentException("Function not found: " + token);
        return function;
    }

    private List<PlanRow> readPlan(File path) throws Exception {
        List<PlanRow> rows = new ArrayList<PlanRow>();
        try (BufferedReader in = new BufferedReader(new InputStreamReader(
                new FileInputStream(path), StandardCharsets.UTF_8))) {
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
        out.write("format\tsource_id\tfunction_entry\tcall_index\tcall_address\treceiver_token\t" +
            "receiver_type_display\tload_present\tload_address_token\tload_address_type_display\t" +
            "load_address_loaded_type_display\taddress_pattern\tbase_token\tbase_type_display\t" +
            "byte_offset\tcomponent_field_name\tcomponent_type_display\t" +
            "provenance_node_count\taddress_patterns_json\tprovenance_trace_json\t" +
            "typed_structure_fields_json\tcandidate_single_pointer_pointee_names_json\t" +
            "candidate_single_pointer_pointee_displays_json\tstatus");
        out.newLine();
    }

    private void writeCall(BufferedWriter out, String sourceId, Function function, int callIndex,
            PcodeOpAST call) throws Exception {
        SequenceNumber seq = call.getSeqnum();
        Varnode receiver = call.getNumInputs() >= 2 ? call.getInput(1) : null;
        DataType receiverType = highType(receiver);
        PcodeOp receiverDef = receiver == null ? null : receiver.getDef();
        boolean isLoad = receiverDef != null && receiverDef.getOpcode() == PcodeOp.LOAD && receiverDef.getNumInputs() >= 2;

        Varnode loadAddress = isLoad ? receiverDef.getInput(1) : null;
        DataType addressType = highType(loadAddress);
        DataType loadedFromAddressType = dereferenceOnce(addressType);
        ImmediateAddress immediate = isLoad ? decodeImmediateAddress(loadAddress) : new ImmediateAddress("none", null, null);
        DataType immediateBaseType = highType(immediate.base);

        ProvenanceEvidence evidence = new ProvenanceEvidence();
        if (isLoad) {
            traceAddressProvenance(loadAddress, 0L, 0, new HashSet<Varnode>(), evidence);
        }

        String baseToken = evidence.firstBaseToken != null ? evidence.firstBaseToken : nodeToken(immediate.base);
        String baseTypeDisplay = evidence.firstBaseTypeDisplay != null ? evidence.firstBaseTypeDisplay :
            (immediateBaseType == null ? null : immediateBaseType.getDisplayName());
        Long byteOffset = evidence.firstByteOffset != null ? evidence.firstByteOffset : immediate.byteOffset;
        String componentFieldName = evidence.firstComponentFieldName;
        String componentTypeDisplay = evidence.firstComponentTypeDisplay;

        String status;
        if (!isLoad) status = "not_load";
        else if (evidence.candidateDisplays.isEmpty() && evidence.candidateNames.isEmpty()) status = "no_candidate";
        else if (evidence.candidateDisplays.size() > 1 || evidence.candidateNames.size() > 1) status = "multiple_candidates";
        else status = "candidate";

        out.write(FORMAT + "\t" + cell(sourceId) + "\t" + cell(function.getEntryPoint()) + "\t" +
            callIndex + "\t" + cell(seq.getTarget()) + "\t" + nodeToken(receiver) + "\t" +
            cell(receiverType == null ? null : receiverType.getDisplayName()) + "\t" + (isLoad ? 1 : 0) + "\t" +
            nodeToken(loadAddress) + "\t" + cell(addressType == null ? null : addressType.getDisplayName()) + "\t" +
            cell(loadedFromAddressType == null ? null : loadedFromAddressType.getDisplayName()) + "\t" +
            cell(immediate.pattern) + "\t" + cell(baseToken) + "\t" + cell(baseTypeDisplay) + "\t" +
            cell(byteOffset == null ? null : "0x" + Long.toHexString(byteOffset.longValue())) + "\t" +
            cell(componentFieldName) + "\t" + cell(componentTypeDisplay) + "\t" +
            evidence.visitedNodes + "\t" + jsonArray(evidence.patterns) + "\t" +
            jsonArray(evidence.trace) + "\t" + jsonArray(evidence.typedFields) + "\t" +
            jsonArray(evidence.candidateNames) + "\t" + jsonArray(evidence.candidateDisplays) + "\t" + status);
        out.newLine();
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException(
                "Usage: ExportIndirectCallLoadReceiverProvenance.java <plan.tsv> <output.tsv>");
        }

        File planPath = new File(args[0]);
        File outputPath = new File(args[1]);
        File parent = outputPath.getParentFile();
        if (parent != null) parent.mkdirs();
        List<PlanRow> plan = readPlan(planPath);

        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) {
            throw new IllegalStateException("Could not open current program in decompiler");
        }

        int functionsOk = 0;
        int functionsFailed = 0;
        int calls = 0;
        int expectedMismatches = 0;
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(
                new FileOutputStream(outputPath), StandardCharsets.UTF_8))) {
            writeHeader(out);
            for (PlanRow item : plan) {
                if (monitor.isCancelled()) break;
                Function function;
                try {
                    function = resolveFunction(item.sourceEntry);
                } catch (Exception error) {
                    printerr("LOAD_RECEIVER_FUNCTION_NOT_FOUND source=" + item.sourceId + " entry=" + item.sourceEntry);
                    functionsFailed++;
                    continue;
                }

                DecompileResults results = decompiler.decompileFunction(function, 60, monitor);
                if (results == null || !results.decompileCompleted() || results.getHighFunction() == null) {
                    String message = results == null ? "no result" : results.getErrorMessage();
                    printerr("LOAD_RECEIVER_DECOMPILE_FAILED source=" + item.sourceId + " message=" + cell(message));
                    functionsFailed++;
                    continue;
                }

                HighFunction high = results.getHighFunction();
                Iterator<PcodeOpAST> operations = high.getPcodeOps();
                int callIndex = 0;
                while (operations.hasNext() && !monitor.isCancelled()) {
                    PcodeOpAST op = operations.next();
                    if (op.getOpcode() != PcodeOp.CALLIND || op.getNumInputs() < 1) continue;
                    writeCall(out, item.sourceId, function, callIndex, op);
                    callIndex++;
                    calls++;
                }
                if (item.expectedCalls >= 0 && item.expectedCalls != callIndex) {
                    expectedMismatches++;
                    printerr("LOAD_RECEIVER_CALL_COUNT_MISMATCH source=" + item.sourceId +
                        " expected=" + item.expectedCalls + " actual=" + callIndex);
                }
                functionsOk++;
            }
        } finally {
            decompiler.dispose();
        }

        println("LOAD_RECEIVER_PROVENANCE_FORMAT=" + FORMAT);
        println("LOAD_RECEIVER_PLAN_FUNCTIONS=" + plan.size());
        println("LOAD_RECEIVER_FUNCTIONS_OK=" + functionsOk);
        println("LOAD_RECEIVER_FUNCTIONS_FAILED=" + functionsFailed);
        println("LOAD_RECEIVER_CALLS=" + calls);
        println("LOAD_RECEIVER_EXPECTED_CALL_MISMATCHES=" + expectedMismatches);
        println("LOAD_RECEIVER_OUTPUT=" + outputPath.getAbsolutePath());
    }
}
