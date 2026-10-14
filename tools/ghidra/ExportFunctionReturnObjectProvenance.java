import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.Pointer;
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
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

// Evidence-only exporter for unresolved structural receivers whose root is a CALL.
// It slices callee RETURN values through COPY/CAST/INDIRECT/MULTIEQUAL and records
// every direct call plus the transparent root of its first C++ argument. The
// downstream audit can therefore prove a returned allocation was passed as `this`
// to a constructor without inferring a class from a virtual target.
public class ExportFunctionReturnObjectProvenance extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraFunctionReturnObjectProvenance/1";
    private static final int MAX_DEPTH = 16;
    private static final int MAX_NODES = 128;
    private static final int MAX_TRANSPARENT_DEPTH = 8;

    private static class PlanRow {
        String sourceId;
        int sourceCallIndex;
        String calleeEntry;
        String calleeName;
    }

    private static class Pending {
        Varnode node;
        int returnIndex;
        int nodeId;
        int parentNodeId;
        int inputIndex;
        int depth;
        Set<String> ancestry;
        Pending(Varnode node, int returnIndex, int nodeId, int parentNodeId, int inputIndex,
                int depth, Set<String> ancestry) {
            this.node = node;
            this.returnIndex = returnIndex;
            this.nodeId = nodeId;
            this.parentNodeId = parentNodeId;
            this.inputIndex = inputIndex;
            this.depth = depth;
            this.ancestry = ancestry;
        }
    }

    private static class TypeInfo {
        int pointerDepth;
        DataType pointee;
        TypeInfo(int pointerDepth, DataType pointee) {
            this.pointerDepth = pointerDepth;
            this.pointee = pointee;
        }
    }

    private String cell(Object value) {
        if (value == null) return "-";
        return String.valueOf(value).replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private DataType unwrap(DataType type) {
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
        DataType cursor = unwrap(original);
        int depth = 0;
        DataType pointee = null;
        while (cursor instanceof Pointer) {
            depth++;
            DataType next = unwrap(((Pointer) cursor).getDataType());
            if (depth == 1) pointee = next;
            if (next == null || next == cursor) break;
            cursor = next;
        }
        return new TypeInfo(depth, pointee);
    }

    private DataType highType(Varnode node) {
        if (node == null) return null;
        HighVariable high = node.getHigh();
        return high == null ? null : high.getDataType();
    }

    private boolean transparent(PcodeOp op) {
        if (op == null) return false;
        int code = op.getOpcode();
        return code == PcodeOp.COPY || code == PcodeOp.CAST || code == PcodeOp.INDIRECT;
    }

    private Varnode stripTransparent(Varnode original) {
        Varnode cursor = original;
        HashSet<Varnode> seen = new HashSet<Varnode>();
        for (int depth = 0; cursor != null && depth < MAX_TRANSPARENT_DEPTH && seen.add(cursor); depth++) {
            PcodeOp def = cursor.getDef();
            if (!transparent(def) || def.getNumInputs() < 1) break;
            cursor = def.getInput(0);
        }
        return cursor;
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

    private String rootToken(Varnode node) {
        return nodeToken(stripTransparent(node));
    }

    private Function directTarget(PcodeOp op) {
        if (op == null || op.getNumInputs() < 1) return null;
        Varnode target = stripTransparent(op.getInput(0));
        if (target == null) return null;
        Address address = null;
        try {
            if (target.isAddress()) address = target.getAddress();
            else if (target.isConstant()) address = toAddr(Long.toHexString(target.getOffset()));
        } catch (Exception ignored) {}
        if (address == null) return null;
        Function function = currentProgram.getFunctionManager().getFunctionAt(address);
        if (function == null) function = currentProgram.getFunctionManager().getFunctionContaining(address);
        return function;
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
                if (first) {
                    first = false;
                    if (cols.length >= 7 && "source_id".equals(cols[0])) continue;
                }
                if (cols.length < 7) throw new IllegalArgumentException("Invalid plan row: " + line);
                PlanRow row = new PlanRow();
                row.sourceId = cols[0];
                row.sourceCallIndex = Integer.parseInt(cols[1]);
                row.calleeEntry = cols[5];
                row.calleeName = cols[6];
                rows.add(row);
            }
        }
        return rows;
    }

    private void writeHeader(BufferedWriter out) throws Exception {
        out.write("format\trow_type\tsource_id\tsource_call_index\tcallee_entry\tcallee_name\tcallee_declared_return_type\t" +
            "return_index\tnode_id\tparent_node_id\tinput_index\tdepth\ttoken\ttype_display\tpointer_depth\tpointee_type_display\t" +
            "constant_value\tdefining_opcode\tdefining_address\tdefining_time\tcall_target_entry\tcall_target_name\t" +
            "call_arg0_root_token\tcall_output_root_token\ttruncation");
        out.newLine();
    }

    private void writeRow(BufferedWriter out, String rowType, PlanRow plan, Function callee,
            int returnIndex, int nodeId, int parentNodeId, int inputIndex, int depth,
            Varnode node, String truncation, PcodeOp explicitCall) throws Exception {
        DataType nodeType = highType(node);
        TypeInfo info = inspectType(nodeType);
        PcodeOp def = explicitCall != null ? explicitCall : (node == null ? null : node.getDef());
        SequenceNumber seq = def == null ? null : def.getSeqnum();
        Function target = def != null && (def.getOpcode() == PcodeOp.CALL || def.getOpcode() == PcodeOp.CALLIND)
            ? directTarget(def) : null;
        String constantValue = node != null && node.isConstant()
            ? "0x" + Long.toUnsignedString(node.getOffset(), 16) : "-";
        String arg0Root = "-";
        String outputRoot = "-";
        if (explicitCall != null) {
            if (explicitCall.getNumInputs() >= 2) arg0Root = rootToken(explicitCall.getInput(1));
            outputRoot = rootToken(explicitCall.getOutput());
        } else if (def != null && (def.getOpcode() == PcodeOp.CALL || def.getOpcode() == PcodeOp.CALLIND)) {
            if (def.getNumInputs() >= 2) arg0Root = rootToken(def.getInput(1));
            outputRoot = rootToken(def.getOutput());
        }
        out.write(FORMAT + "\t" + rowType + "\t" + cell(plan.sourceId) + "\t" + plan.sourceCallIndex + "\t" +
            cell(callee.getEntryPoint()) + "\t" + cell(callee.getName(true)) + "\t" + cell(callee.getReturnType()) + "\t" +
            returnIndex + "\t" + nodeId + "\t" + parentNodeId + "\t" + inputIndex + "\t" + depth + "\t" +
            nodeToken(node) + "\t" + cell(nodeType == null ? null : nodeType.getDisplayName()) + "\t" + info.pointerDepth + "\t" +
            cell(info.pointee == null ? null : info.pointee.getDisplayName()) + "\t" + constantValue + "\t" +
            cell(def == null ? null : def.getMnemonic()) + "\t" + cell(seq == null ? null : seq.getTarget()) + "\t" +
            cell(seq == null ? null : seq.getTime()) + "\t" + cell(target == null ? null : target.getEntryPoint()) + "\t" +
            cell(target == null ? null : target.getName(true)) + "\t" + arg0Root + "\t" + outputRoot + "\t" + cell(truncation));
        out.newLine();
    }

    private int writeReturnSlice(BufferedWriter out, PlanRow plan, Function callee, int returnIndex, Varnode value) throws Exception {
        ArrayDeque<Pending> queue = new ArrayDeque<Pending>();
        HashSet<String> initial = new HashSet<String>();
        queue.add(new Pending(value, returnIndex, 0, -1, -1, 0, initial));
        int nextNodeId = 1;
        int written = 0;
        while (!queue.isEmpty() && written < MAX_NODES && !monitor.isCancelled()) {
            Pending item = queue.removeFirst();
            String token = nodeToken(item.node);
            boolean cycle = item.ancestry.contains(token);
            String truncation = cycle ? "cycle" : (item.depth >= MAX_DEPTH ? "depth" : "-");
            writeRow(out, "return_node", plan, callee, item.returnIndex, item.nodeId,
                item.parentNodeId, item.inputIndex, item.depth, item.node, truncation, null);
            written++;
            if (cycle || item.depth >= MAX_DEPTH || item.node == null) continue;
            PcodeOp def = item.node.getDef();
            if (def == null) continue;
            ArrayList<Integer> expand = new ArrayList<Integer>();
            int code = def.getOpcode();
            if (code == PcodeOp.COPY || code == PcodeOp.CAST || code == PcodeOp.INDIRECT) {
                if (def.getNumInputs() >= 1) expand.add(0);
            } else if (code == PcodeOp.MULTIEQUAL) {
                for (int i = 0; i < def.getNumInputs(); i++) expand.add(i);
            }
            if (expand.isEmpty()) continue;
            HashSet<String> ancestry = new HashSet<String>(item.ancestry);
            ancestry.add(token);
            for (Integer input : expand) {
                Varnode child = def.getInput(input);
                queue.addLast(new Pending(child, returnIndex, nextNodeId++, item.nodeId,
                    input, item.depth + 1, ancestry));
            }
        }
        return written;
    }

    private int runOne(BufferedWriter out, DecompInterface decompiler, PlanRow plan) throws Exception {
        Function callee = resolveFunction(plan.calleeEntry);
        DecompileResults results = decompiler.decompileFunction(callee, 60, monitor);
        if (results == null || !results.decompileCompleted() || results.getHighFunction() == null)
            throw new IllegalStateException("Could not decompile " + plan.calleeEntry);
        HighFunction high = results.getHighFunction();
        writeRow(out, "summary", plan, callee, -1, -1, -1, -1, -1, null, "-", null);

        int directCalls = 0;
        int returns = 0;
        int returnNodes = 0;
        Iterator<PcodeOpAST> it = high.getPcodeOps();
        while (it.hasNext() && !monitor.isCancelled()) {
            PcodeOpAST op = it.next();
            if (op.getOpcode() == PcodeOp.CALL) {
                writeRow(out, "call", plan, callee, -1, -1, -1, -1, -1, null, "-", op);
                directCalls++;
            } else if (op.getOpcode() == PcodeOp.RETURN) {
                Varnode value = op.getNumInputs() >= 2 ? op.getInput(1) : null;
                if (value != null) returnNodes += writeReturnSlice(out, plan, callee, returns, value);
                else writeRow(out, "return_void", plan, callee, returns, -1, -1, -1, -1, null, "-", op);
                returns++;
            }
        }
        println("FACTORY_RETURN_SOURCE=" + plan.sourceId + "#call=" + plan.sourceCallIndex +
            " callee=" + callee.getEntryPoint() + " direct_calls=" + directCalls +
            " returns=" + returns + " return_nodes=" + returnNodes);
        return returns;
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2)
            throw new IllegalArgumentException("Usage: ExportFunctionReturnObjectProvenance.java <plan.tsv> <output.tsv>");
        File planPath = new File(args[0]);
        File outputPath = new File(args[1]);
        File parent = outputPath.getParentFile();
        if (parent != null) parent.mkdirs();
        List<PlanRow> plan = readPlan(planPath);
        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException("Could not open program");
        int totalReturns = 0;
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(outputPath), StandardCharsets.UTF_8))) {
            writeHeader(out);
            for (PlanRow row : plan) totalReturns += runOne(out, decompiler, row);
        } finally {
            decompiler.dispose();
        }
        println("FACTORY_RETURN_FORMAT=" + FORMAT);
        println("FACTORY_RETURN_PLAN_ROWS=" + plan.size());
        println("FACTORY_RETURN_RETURNS=" + totalReturns);
        println("FACTORY_RETURN_OUTPUT=" + outputPath.getAbsolutePath());
        println("FACTORY_RETURN_AFFECTS_ROUTING=no");
    }
}
