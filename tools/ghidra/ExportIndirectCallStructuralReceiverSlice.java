import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.Pointer;
import ghidra.program.model.data.TypeDef;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.HighVariable;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.PcodeOpAST;
import ghidra.program.model.pcode.SequenceNumber;
import ghidra.program.model.pcode.Varnode;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

// Evidence-only bounded slice of the receiver recovered from the already validated
// canonical virtual-dispatch target shape:
//   CALLIND(LOAD(LOAD(receiver) + constant_slot))
//
// Unlike ExportIndirectCallSlice, this walks the recovered receiver rather than the
// indirect target. It is intended to inspect MULTIEQUAL/CALL roots of unresolved
// receiver_not_single_pointer rows. It never changes program state or routing.
public class ExportIndirectCallStructuralReceiverSlice extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraStructuralReceiverSlice/1";
    private static final int DEFAULT_MAX_NODES = 96;
    private static final int DEFAULT_MAX_DEPTH = 16;
    private static final int MAX_TRANSPARENT_DEPTH = 6;

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

    private static class ReceiverRecovery {
        final Varnode receiver;
        final Long slotOffset;
        final String status;
        ReceiverRecovery(Varnode receiver, Long slotOffset, String status) {
            this.receiver = receiver;
            this.slotOffset = slotOffset;
            this.status = status;
        }
    }

    private static class PendingNode {
        final Varnode node;
        final int nodeId;
        final int parentNodeId;
        final int inputIndex;
        final int depth;
        final Set<String> ancestry;
        final boolean cycle;
        PendingNode(Varnode node, int nodeId, int parentNodeId, int inputIndex, int depth,
                Set<String> ancestry, boolean cycle) {
            this.node = node;
            this.nodeId = nodeId;
            this.parentNodeId = parentNodeId;
            this.inputIndex = inputIndex;
            this.depth = depth;
            this.ancestry = ancestry;
            this.cycle = cycle;
        }
    }

    private static class SliceNode {
        int nodeId;
        int parentNodeId;
        int inputIndex;
        int depth;
        String token;
        String typeDisplay;
        int pointerDepth;
        String pointeeType;
        String constantValue;
        String definingOpcode;
        String definingAddress;
        long definingTime;
        String callTargetEntry;
        String callTargetName;
        String callTargetReturnType;
        String callTargetIsThunk;
        String callTargetThunkTarget;
        String truncation;
    }

    private static class SliceResult {
        final List<SliceNode> nodes = new ArrayList<SliceNode>();
        boolean truncatedByBudget = false;
        int depthTruncationCount = 0;
        int cycleCount = 0;
    }

    private String cell(Object value) {
        if (value == null) return "-";
        return String.valueOf(value).replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private Function resolveFunction(String token) throws Exception {
        Address address = null;
        try { address = toAddr(token); } catch (Exception ignored) {}
        if (address != null) {
            Function function = currentProgram.getFunctionManager().getFunctionAt(address);
            if (function == null) function = currentProgram.getFunctionManager().getFunctionContaining(address);
            if (function != null) return function;
        }

        Function exact = null;
        Function partial = null;
        int partialCount = 0;
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        while (it.hasNext() && !monitor.isCancelled()) {
            Function function = it.next();
            String shortName = function.getName();
            String fullName = function.getName(true);
            if (token.equals(shortName) || token.equals(fullName)) {
                exact = function;
                break;
            }
            if (shortName.contains(token) || fullName.contains(token)) {
                partial = function;
                partialCount++;
            }
        }
        if (exact != null) return exact;
        if (partialCount == 1) return partial;
        if (partialCount > 1) throw new IllegalArgumentException("Ambiguous function token: " + token);
        throw new IllegalArgumentException("Function not found: " + token);
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

    private String constantValue(Varnode node) {
        if (node == null || !node.isConstant()) return "-";
        return "0x" + Long.toUnsignedString(node.getOffset(), 16);
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

    private Varnode stripTransparent(Varnode original) {
        Varnode cursor = original;
        HashSet<Varnode> seen = new HashSet<Varnode>();
        for (int depth = 0; cursor != null && depth < MAX_TRANSPARENT_DEPTH && seen.add(cursor); depth++) {
            PcodeOp def = cursor.getDef();
            if (!isTransparent(def) || def.getNumInputs() < 1) break;
            cursor = def.getInput(0);
        }
        return cursor;
    }

    private boolean constantInput(PcodeOp op, int index) {
        return op != null && op.getNumInputs() > index && op.getInput(index) != null && op.getInput(index).isConstant();
    }

    private TargetBase decodeTargetAddress(Varnode original) {
        Varnode cursor = stripTransparent(original);
        PcodeOp def = cursor == null ? null : cursor.getDef();
        if (def == null) return new TargetBase(cursor, 0L, "DIRECT");
        int opcode = def.getOpcode();
        try {
            if (opcode == PcodeOp.PTRSUB && def.getNumInputs() >= 2 && constantInput(def, 1))
                return new TargetBase(def.getInput(0), def.getInput(1).getOffset(), "PTRSUB");
            if (opcode == PcodeOp.PTRADD && def.getNumInputs() >= 3 && constantInput(def, 1) && constantInput(def, 2)) {
                long offset = Math.multiplyExact(def.getInput(1).getOffset(), def.getInput(2).getOffset());
                return new TargetBase(def.getInput(0), offset, "PTRADD");
            }
            if (opcode == PcodeOp.INT_ADD && def.getNumInputs() >= 2) {
                if (constantInput(def, 1)) return new TargetBase(def.getInput(0), def.getInput(1).getOffset(), "INT_ADD");
                if (constantInput(def, 0)) return new TargetBase(def.getInput(1), def.getInput(0).getOffset(), "INT_ADD");
            }
            if (opcode == PcodeOp.INT_SUB && def.getNumInputs() >= 2 && constantInput(def, 1))
                return new TargetBase(def.getInput(0), -def.getInput(1).getOffset(), "INT_SUB");
        } catch (ArithmeticException ignored) {
            return new TargetBase(null, null, "OFFSET_OVERFLOW");
        }
        return new TargetBase(null, null, def.getMnemonic());
    }

    private ReceiverRecovery recoverReceiver(PcodeOpAST call) {
        if (call == null || call.getOpcode() != PcodeOp.CALLIND || call.getNumInputs() < 1)
            return new ReceiverRecovery(null, null, "not_callind");
        Varnode target = stripTransparent(call.getInput(0));
        PcodeOp targetDef = target == null ? null : target.getDef();
        if (targetDef == null || targetDef.getOpcode() != PcodeOp.LOAD || targetDef.getNumInputs() < 2)
            return new ReceiverRecovery(null, null, "target_not_load");
        TargetBase decoded = decodeTargetAddress(targetDef.getInput(1));
        if (decoded.base == null)
            return new ReceiverRecovery(null, decoded.offset, "target_address_not_constant_form");
        Varnode vtable = stripTransparent(decoded.base);
        PcodeOp vtableDef = vtable == null ? null : vtable.getDef();
        if (vtableDef == null || vtableDef.getOpcode() != PcodeOp.LOAD || vtableDef.getNumInputs() < 2)
            return new ReceiverRecovery(null, decoded.offset, "vtable_not_load");
        Varnode receiver = stripTransparent(vtableDef.getInput(1));
        return new ReceiverRecovery(receiver, decoded.offset, receiver == null ? "receiver_missing" : "canonical_receiver");
    }

    private Address callTargetAddress(PcodeOp op) {
        if (op == null || op.getOpcode() != PcodeOp.CALL || op.getNumInputs() < 1) return null;
        Varnode target = op.getInput(0);
        if (target == null) return null;
        if (!target.isConstant()) return target.getAddress();
        try { return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(target.getOffset()); }
        catch (Exception ignored) { return null; }
    }

    private Function resolveCallTarget(PcodeOp op) {
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

    private String thunkTarget(Function function) {
        if (function == null || !function.isThunk()) return "-";
        try {
            Function target = function.getThunkedFunction(true);
            if (target == null) return "-";
            return cell(target.getEntryPoint()) + ":" + cell(target.getName(true));
        } catch (Exception ignored) { return "-"; }
    }

    private int firstDataInput(PcodeOp def) {
        if (def == null) return 0;
        if (def.getOpcode() == PcodeOp.LOAD) return Math.min(1, def.getNumInputs());
        return 0;
    }

    private int dataInputCount(PcodeOp def) {
        if (def == null) return 0;
        if (def.getOpcode() == PcodeOp.INDIRECT) return Math.min(1, def.getNumInputs());
        if (def.getOpcode() == PcodeOp.CALL || def.getOpcode() == PcodeOp.CALLIND) return 0;
        return def.getNumInputs() - firstDataInput(def);
    }

    private SliceResult buildSlice(Varnode root, int maxNodes, int maxDepth) {
        SliceResult result = new SliceResult();
        if (root == null) return result;
        ArrayDeque<PendingNode> queue = new ArrayDeque<PendingNode>();
        String rootToken = nodeToken(root);
        Set<String> rootAncestry = new HashSet<String>();
        rootAncestry.add(rootToken);
        queue.add(new PendingNode(root, 0, -1, -1, 0, rootAncestry, false));
        int nextNodeId = 1;

        while (!queue.isEmpty()) {
            if (result.nodes.size() >= maxNodes) {
                result.truncatedByBudget = true;
                break;
            }
            PendingNode pending = queue.removeFirst();
            Varnode node = pending.node;
            PcodeOp def = node == null ? null : node.getDef();
            DataType type = highType(node);
            TypeInfo typeInfo = inspectType(type);
            SliceNode out = new SliceNode();
            out.nodeId = pending.nodeId;
            out.parentNodeId = pending.parentNodeId;
            out.inputIndex = pending.inputIndex;
            out.depth = pending.depth;
            out.token = nodeToken(node);
            out.typeDisplay = cell(type == null ? null : type.getDisplayName());
            out.pointerDepth = typeInfo.pointerDepth;
            out.pointeeType = cell(typeInfo.pointee == null ? null : typeInfo.pointee.getDisplayName());
            out.constantValue = constantValue(node);
            out.definingOpcode = def == null ? "-" : def.getMnemonic();
            out.definingAddress = def == null ? "-" : cell(def.getSeqnum().getTarget());
            out.definingTime = def == null ? -1 : def.getSeqnum().getTime();
            out.callTargetEntry = "-";
            out.callTargetName = "-";
            out.callTargetReturnType = "-";
            out.callTargetIsThunk = "-";
            out.callTargetThunkTarget = "-";
            out.truncation = "-";

            if (def != null && def.getOpcode() == PcodeOp.CALL) {
                Function target = resolveCallTarget(def);
                if (target != null) {
                    out.callTargetEntry = cell(target.getEntryPoint());
                    out.callTargetName = cell(target.getName(true));
                    DataType returnType = target.getReturnType();
                    out.callTargetReturnType = cell(returnType == null ? null : returnType.getDisplayName());
                    out.callTargetIsThunk = target.isThunk() ? "1" : "0";
                    out.callTargetThunkTarget = thunkTarget(target);
                }
            }

            if (pending.cycle) {
                out.truncation = "cycle";
                result.cycleCount++;
                result.nodes.add(out);
                continue;
            }

            int childCount = dataInputCount(def);
            if (pending.depth >= maxDepth && childCount > 0) {
                out.truncation = "depth";
                result.depthTruncationCount++;
                result.nodes.add(out);
                continue;
            }

            result.nodes.add(out);
            if (def == null || childCount == 0) continue;
            int start = firstDataInput(def);
            int end = start + childCount;
            for (int index = start; index < end; index++) {
                Varnode child = def.getInput(index);
                if (child == null) continue;
                String childToken = nodeToken(child);
                boolean cycle = pending.ancestry.contains(childToken);
                Set<String> ancestry = new HashSet<String>(pending.ancestry);
                ancestry.add(childToken);
                queue.addLast(new PendingNode(child, nextNodeId++, pending.nodeId, index,
                    pending.depth + 1, ancestry, cycle));
            }
        }
        return result;
    }

    private void writeSummary(BufferedWriter out, Function function, int callIndex, SequenceNumber seq,
            ReceiverRecovery recovery, SliceResult slice, int maxNodes, int maxDepth) throws Exception {
        out.write(FORMAT + "\tsummary\t" + cell(function.getEntryPoint()) + "\t" + callIndex + "\t" +
            cell(seq.getTarget()) + "\t" + seq.getTime() + "\t" + recovery.status + "\t" +
            (recovery.slotOffset == null ? "-" : "0x" + Long.toHexString(recovery.slotOffset)) + "\t" +
            nodeToken(recovery.receiver) + "\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t" +
            slice.nodes.size() + "\t" + (slice.truncatedByBudget ? 1 : 0) + "\t" +
            slice.depthTruncationCount + "\t" + slice.cycleCount + "\t" + maxNodes + "\t" + maxDepth);
        out.newLine();
    }

    private void writeNode(BufferedWriter out, Function function, int callIndex, SequenceNumber seq,
            ReceiverRecovery recovery, SliceNode node, int maxNodes, int maxDepth) throws Exception {
        out.write(FORMAT + "\tnode\t" + cell(function.getEntryPoint()) + "\t" + callIndex + "\t" +
            cell(seq.getTarget()) + "\t" + seq.getTime() + "\t" + recovery.status + "\t" +
            (recovery.slotOffset == null ? "-" : "0x" + Long.toHexString(recovery.slotOffset)) + "\t" +
            nodeToken(recovery.receiver) + "\t" + node.nodeId + "\t" + node.parentNodeId + "\t" +
            node.inputIndex + "\t" + node.depth + "\t" + cell(node.token) + "\t" + cell(node.typeDisplay) + "\t" +
            node.pointerDepth + "\t" + cell(node.pointeeType) + "\t" + node.constantValue + "\t" +
            cell(node.definingOpcode) + "\t" + cell(node.definingAddress) + "\t" + node.definingTime + "\t" +
            cell(node.callTargetEntry) + "\t" + cell(node.callTargetName) + "\t" + cell(node.callTargetReturnType) + "\t" +
            cell(node.callTargetIsThunk) + "\t" + cell(node.callTargetThunkTarget) + "\t" + node.truncation +
            "\t-\t-\t-\t-\t" + maxNodes + "\t" + maxDepth);
        out.newLine();
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2 || args.length > 4) {
            throw new IllegalArgumentException(
                "Usage: ExportIndirectCallStructuralReceiverSlice.java <function-address-or-name> <output.tsv> [max-nodes] [max-depth]");
        }
        int maxNodes = args.length >= 3 ? Integer.parseInt(args[2]) : DEFAULT_MAX_NODES;
        int maxDepth = args.length >= 4 ? Integer.parseInt(args[3]) : DEFAULT_MAX_DEPTH;
        if (maxNodes < 8) throw new IllegalArgumentException("max-nodes must be >= 8");
        if (maxDepth < 2) throw new IllegalArgumentException("max-depth must be >= 2");

        Function function = resolveFunction(args[0]);
        File output = new File(args[1]);
        File parent = output.getParentFile();
        if (parent != null) parent.mkdirs();

        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException("Could not open current program in decompiler");
        int calls = 0;
        int canonical = 0;
        try {
            DecompileResults results = decompiler.decompileFunction(function, 60, monitor);
            if (results == null || !results.decompileCompleted()) {
                String message = results == null ? "no result" : results.getErrorMessage();
                throw new IllegalStateException("Decompile failed for " + function.getName(true) + ": " + message);
            }
            HighFunction high = results.getHighFunction();
            if (high == null) throw new IllegalStateException("No HighFunction for " + function.getName(true));
            try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(output), StandardCharsets.UTF_8))) {
                out.write("format\trow_type\tfunction_entry\tcall_index\tcall_address\tcall_time\trecovery_status\tslot_offset\tstructural_receiver_token\tnode_id\tparent_node_id\tinput_index\tdepth\tnode_token\ttype_display\tpointer_depth\tpointee_type_display\tconstant_value\tdefining_opcode\tdefining_address\tdefining_time\tcall_target_entry\tcall_target_name\tcall_target_return_type\tcall_target_is_thunk\tcall_target_thunk_target\ttruncation\tnode_count\ttruncated_by_budget\tdepth_truncation_count\tcycle_count\tmax_nodes\tmax_depth");
                out.newLine();
                Iterator<PcodeOpAST> operations = high.getPcodeOps();
                while (operations.hasNext() && !monitor.isCancelled()) {
                    PcodeOpAST op = operations.next();
                    if (op.getOpcode() != PcodeOp.CALLIND || op.getNumInputs() < 1) continue;
                    SequenceNumber seq = op.getSeqnum();
                    ReceiverRecovery recovery = recoverReceiver(op);
                    SliceResult slice = buildSlice(recovery.receiver, maxNodes, maxDepth);
                    writeSummary(out, function, calls, seq, recovery, slice, maxNodes, maxDepth);
                    for (SliceNode node : slice.nodes) writeNode(out, function, calls, seq, recovery, node, maxNodes, maxDepth);
                    if ("canonical_receiver".equals(recovery.status)) canonical++;
                    calls++;
                }
            }
        } finally {
            decompiler.dispose();
        }

        println("STRUCTURAL_RECEIVER_SLICE_FORMAT=" + FORMAT);
        println("STRUCTURAL_RECEIVER_SLICE_CALLS=" + calls);
        println("STRUCTURAL_RECEIVER_SLICE_CANONICAL=" + canonical);
        println("STRUCTURAL_RECEIVER_SLICE_OUTPUT=" + output.getAbsolutePath());
        println("STRUCTURAL_RECEIVER_SLICE_AFFECTS_ROUTING=no");
    }
}
