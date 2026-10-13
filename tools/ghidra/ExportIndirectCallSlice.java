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
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

// Bounded clean-room provenance slices for CALLIND targets. The traversal is
// limited by node count and depth rather than by a small expression-recursion
// depth. It emits only high-P-code data-flow metadata; no decompiler C,
// disassembly, instruction bytes, strings, or source assets are exported.
public class ExportIndirectCallSlice extends GhidraScript {
    private static final int DEFAULT_MAX_NODES = 96;
    private static final int DEFAULT_MAX_DEPTH = 24;

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
        String constantValue;
        String definingOpcode;
        String definingAddress;
        long definingTime;
        String truncation;
    }

    private static class SliceResult {
        final List<SliceNode> nodes = new ArrayList<SliceNode>();
        boolean truncatedByBudget = false;
        int depthTruncationCount = 0;
        int cycleCount = 0;
    }

    private String cell(Object value) {
        return String.valueOf(value).replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private Function resolveFunction(String token) throws Exception {
        Address address = null;
        try {
            address = toAddr(token);
        } catch (Exception ignored) {
            // Fall through to name lookup.
        }
        if (address != null) {
            Function function = currentProgram.getFunctionManager().getFunctionAt(address);
            if (function == null) {
                function = currentProgram.getFunctionManager().getFunctionContaining(address);
            }
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
        if (node == null) return "none";
        if (node.isConstant()) {
            return "const:0x" + Long.toUnsignedString(node.getOffset(), 16) + ":" + node.getSize();
        }
        PcodeOp def = node.getDef();
        if (def != null) {
            SequenceNumber seq = def.getSeqnum();
            return "def:" + def.getMnemonic() + "@" + cell(seq.getTarget()) + ":" +
                seq.getTime() + ":" + node.getSize();
        }
        if (node.isRegister()) return "register:" + cell(node.getAddress()) + ":" + node.getSize();
        if (node.isUnique()) return "unique:" + cell(node.getAddress()) + ":" + node.getSize();
        if (node.isAddress()) return "address:" + cell(node.getAddress()) + ":" + node.getSize();
        return "input:" + cell(node.getAddress()) + ":" + node.getSize();
    }

    private String constantValue(Varnode node) {
        if (node == null || !node.isConstant()) return "-";
        return "0x" + Long.toUnsignedString(node.getOffset(), 16);
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
            SliceNode out = new SliceNode();
            out.nodeId = pending.nodeId;
            out.parentNodeId = pending.parentNodeId;
            out.inputIndex = pending.inputIndex;
            out.depth = pending.depth;
            out.token = nodeToken(node);
            out.constantValue = constantValue(node);
            out.definingOpcode = def == null ? "-" : def.getMnemonic();
            out.definingAddress = def == null ? "-" : cell(def.getSeqnum().getTarget());
            out.definingTime = def == null ? -1 : def.getSeqnum().getTime();
            out.truncation = "-";

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
                queue.addLast(new PendingNode(
                    child, nextNodeId++, pending.nodeId, index, pending.depth + 1, ancestry, cycle));
            }
        }
        return result;
    }

    private void writeSummary(BufferedWriter out, Function function, int callIndex,
            SequenceNumber seq, int argCount, SliceResult slice, int maxNodes, int maxDepth)
            throws Exception {
        out.write("summary\t" + cell(function.getEntryPoint()) + "\t" + callIndex + "\t" +
            cell(seq.getTarget()) + "\t" + seq.getTime() + "\t" + argCount +
            "\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t" + slice.nodes.size() + "\t" +
            (slice.truncatedByBudget ? 1 : 0) + "\t" + slice.depthTruncationCount + "\t" +
            slice.cycleCount + "\t" + maxNodes + "\t" + maxDepth);
        out.newLine();
    }

    private void writeNode(BufferedWriter out, Function function, int callIndex,
            SequenceNumber seq, int argCount, SliceNode node, int maxNodes, int maxDepth)
            throws Exception {
        out.write("node\t" + cell(function.getEntryPoint()) + "\t" + callIndex + "\t" +
            cell(seq.getTarget()) + "\t" + seq.getTime() + "\t" + argCount + "\t" +
            node.nodeId + "\t" + node.parentNodeId + "\t" + node.inputIndex + "\t" +
            node.depth + "\t" + cell(node.token) + "\t" + node.constantValue + "\t" +
            cell(node.definingOpcode) + "\t" + cell(node.definingAddress) + "\t" +
            node.definingTime + "\t" + node.truncation + "\t-\t-\t-\t-\t" +
            maxNodes + "\t" + maxDepth);
        out.newLine();
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2 || args.length > 4) {
            throw new IllegalArgumentException(
                "Usage: ExportIndirectCallSlice.java <function-address-or-name> <output.tsv> [max-nodes] [max-depth]");
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
        if (!decompiler.openProgram(currentProgram)) {
            throw new IllegalStateException("Could not open current program in decompiler");
        }

        int calls = 0;
        try {
            DecompileResults results = decompiler.decompileFunction(function, 60, monitor);
            if (results == null || !results.decompileCompleted()) {
                String message = results == null ? "no result" : results.getErrorMessage();
                throw new IllegalStateException("Decompile failed for " + function.getName(true) + ": " + message);
            }
            HighFunction high = results.getHighFunction();
            if (high == null) throw new IllegalStateException("No HighFunction for " + function.getName(true));

            try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(
                    new FileOutputStream(output), StandardCharsets.UTF_8))) {
                out.write("row_type\tfunction_entry\tcall_index\tcall_address\tcall_time\targ_count\tnode_id\tparent_node_id\tinput_index\tdepth\tnode_token\tconstant_value\tdefining_opcode\tdefining_address\tdefining_time\ttruncation\tnode_count\ttruncated_by_budget\tdepth_truncation_count\tcycle_count\tmax_nodes\tmax_depth");
                out.newLine();

                Iterator<PcodeOpAST> operations = high.getPcodeOps();
                while (operations.hasNext() && !monitor.isCancelled()) {
                    PcodeOpAST op = operations.next();
                    if (op.getOpcode() != PcodeOp.CALLIND || op.getNumInputs() < 1) continue;
                    SequenceNumber seq = op.getSeqnum();
                    SliceResult slice = buildSlice(op.getInput(0), maxNodes, maxDepth);
                    int argCount = Math.max(0, op.getNumInputs() - 1);
                    writeSummary(out, function, calls, seq, argCount, slice, maxNodes, maxDepth);
                    for (SliceNode node : slice.nodes) {
                        writeNode(out, function, calls, seq, argCount, node, maxNodes, maxDepth);
                    }
                    calls++;
                }
            }
        } finally {
            decompiler.dispose();
        }

        println("Exported bounded slices for " + calls + " indirect calls in " +
            function.getName(true) + " at " + function.getEntryPoint() + " -> " + output +
            " (maxNodes=" + maxNodes + ", maxDepth=" + maxDepth + ")");
    }
}
