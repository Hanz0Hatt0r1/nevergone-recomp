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
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

// Bounded clean-room evidence for CALLIND targets. This exporter records high
// P-code data-flow only; it does not emit instruction bytes, disassembly,
// decompiler C, strings, or source assets.
public class ExportIndirectCallEvidence extends GhidraScript {
    private static final int MAX_DEPTH = 6;

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
            return "def:" + def.getMnemonic() + "@" + cell(seq.getTarget()) + ":" + seq.getTime() + ":" + node.getSize();
        }
        if (node.isRegister()) return "register:" + cell(node.getAddress()) + ":" + node.getSize();
        if (node.isUnique()) return "unique:" + cell(node.getAddress()) + ":" + node.getSize();
        if (node.isAddress()) return "address:" + cell(node.getAddress()) + ":" + node.getSize();
        return "input:" + cell(node.getAddress()) + ":" + node.getSize();
    }

    private String expression(Varnode node, int depth, Set<String> seen) {
        if (node == null) return "none";
        String token = nodeToken(node);
        if (depth >= MAX_DEPTH) return token + "{depth-limit}";
        if (!seen.add(token)) return token + "{cycle}";

        PcodeOp def = node.getDef();
        if (def == null) return token;
        int opcode = def.getOpcode();
        StringBuilder out = new StringBuilder();
        out.append(def.getMnemonic()).append('(');

        int start = 0;
        if (opcode == PcodeOp.LOAD) start = Math.min(1, def.getNumInputs());
        if (opcode == PcodeOp.INDIRECT) {
            int count = Math.min(1, def.getNumInputs());
            for (int i = 0; i < count; i++) {
                if (i > 0) out.append(',');
                out.append(expression(def.getInput(i), depth + 1, new HashSet<String>(seen)));
            }
            out.append(')');
            return out.toString();
        }

        for (int i = start; i < def.getNumInputs(); i++) {
            if (i > start) out.append(',');
            out.append(expression(def.getInput(i), depth + 1, new HashSet<String>(seen)));
        }
        out.append(')');
        return out.toString();
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException(
                "Usage: ExportIndirectCallEvidence.java <function-address-or-name> <output.tsv>");
        }

        Function function = resolveFunction(args[0]);
        File output = new File(args[1]);
        File parent = output.getParentFile();
        if (parent != null) parent.mkdirs();

        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) {
            throw new IllegalStateException("Could not open current program in decompiler");
        }

        int indirectCalls = 0;
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
                out.write("function_entry\tcall_address\tcall_time\ttarget\ttarget_expression\targ_count");
                out.newLine();
                Iterator<PcodeOpAST> operations = high.getPcodeOps();
                while (operations.hasNext() && !monitor.isCancelled()) {
                    PcodeOpAST op = operations.next();
                    if (op.getOpcode() != PcodeOp.CALLIND || op.getNumInputs() < 1) continue;
                    SequenceNumber seq = op.getSeqnum();
                    Varnode target = op.getInput(0);
                    String expr = expression(target, 0, new HashSet<String>());
                    out.write(cell(function.getEntryPoint()) + "\t" + cell(seq.getTarget()) + "\t" +
                        seq.getTime() + "\t" + cell(nodeToken(target)) + "\t" + cell(expr) + "\t" +
                        Math.max(0, op.getNumInputs() - 1));
                    out.newLine();
                    indirectCalls++;
                }
            }
        } finally {
            decompiler.dispose();
        }

        println("Exported " + indirectCalls + " indirect-call rows for " + function.getName(true) +
            " at " + function.getEntryPoint() + " -> " + output);
    }
}
