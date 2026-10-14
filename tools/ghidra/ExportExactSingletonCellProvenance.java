import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.PcodeOpAST;
import ghidra.program.model.pcode.SequenceNumber;
import ghidra.program.model.pcode.Varnode;
import ghidra.program.model.symbol.Reference;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

// Evidence-only exporter for one already identified singleton accessor and one
// exact singleton cell. STORE/LOAD rows are emitted only when the instruction
// containing the high-P-code op has a direct low-level reference to that exact
// cell. This deliberately avoids folding P-code address expressions.
public class ExportExactSingletonCellProvenance extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraExactSingletonCellProvenance/1";
    private static final int MAX_TRANSPARENT_DEPTH = 10;

    private String cell(Object value) {
        if (value == null) return "-";
        return String.valueOf(value).replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private boolean transparent(PcodeOp op) {
        if (op == null) return false;
        int code = op.getOpcode();
        return code == PcodeOp.COPY || code == PcodeOp.CAST || code == PcodeOp.INDIRECT;
    }

    private Varnode stripTransparent(Varnode original) {
        Varnode cursor = original;
        Set<Varnode> seen = new HashSet<Varnode>();
        for (int depth = 0; cursor != null && depth < MAX_TRANSPARENT_DEPTH && seen.add(cursor); depth++) {
            PcodeOp def = cursor.getDef();
            if (!transparent(def) || def.getNumInputs() < 1) break;
            cursor = def.getInput(0);
        }
        return cursor;
    }

    private String token(Varnode node) {
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

    private String rootToken(Varnode node) {
        return token(stripTransparent(node));
    }

    private Function resolveFunction(String text) throws Exception {
        try {
            Address address = toAddr(text);
            Function at = currentProgram.getFunctionManager().getFunctionAt(address);
            if (at == null) at = currentProgram.getFunctionManager().getFunctionContaining(address);
            if (at != null) return at;
        } catch (Exception ignored) {}

        Function exact = null;
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        while (it.hasNext() && !monitor.isCancelled()) {
            Function function = it.next();
            if (text.equals(function.getName()) || text.equals(function.getName(true))) {
                if (exact != null && !exact.getEntryPoint().equals(function.getEntryPoint())) {
                    throw new IllegalArgumentException("Ambiguous exact function name: " + text);
                }
                exact = function;
            }
        }
        if (exact == null) throw new IllegalArgumentException("Function not found: " + text);
        return exact;
    }

    private Function directTarget(PcodeOp op) {
        if (op == null || op.getOpcode() != PcodeOp.CALL || op.getNumInputs() < 1) return null;
        Varnode target = stripTransparent(op.getInput(0));
        if (target == null) return null;
        Address address = null;
        try {
            if (target.isAddress()) address = target.getAddress();
            else if (target.isConstant()) {
                address = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(target.getOffset());
            }
        } catch (Exception ignored) {}
        if (address == null) return null;
        Function function = currentProgram.getFunctionManager().getFunctionAt(address);
        if (function == null && (address.getOffset() & 1L) != 0L) {
            try {
                Address even = address.getAddressSpace().getAddress(address.getOffset() & ~1L);
                function = currentProgram.getFunctionManager().getFunctionAt(even);
            } catch (Exception ignored) {}
        }
        if (function == null) function = currentProgram.getFunctionManager().getFunctionContaining(address);
        if (function != null && function.isThunk()) {
            try {
                Function targetFunction = function.getThunkedFunction(true);
                if (targetFunction != null) function = targetFunction;
            } catch (Exception ignored) {}
        }
        return function;
    }

    private boolean instructionReferencesCell(PcodeOp op, Address targetCell) {
        if (op == null || op.getSeqnum() == null || op.getSeqnum().getTarget() == null) return false;
        Instruction instruction = currentProgram.getListing().getInstructionAt(op.getSeqnum().getTarget());
        if (instruction == null) {
            instruction = currentProgram.getListing().getInstructionContaining(op.getSeqnum().getTarget());
        }
        if (instruction == null) return false;
        for (Reference reference : instruction.getReferencesFrom()) {
            if (reference.getToAddress() != null && reference.getToAddress().equals(targetCell)) return true;
        }
        return false;
    }

    private void header(BufferedWriter out) throws Exception {
        out.write("format\tsource_id\tcall_index\tfunction_entry\tfunction_name\tcell_address\trow_type\t" +
            "op_address\top_time\tvalue_root_token\tcall_target_entry\tcall_target_name\t" +
            "call_arg0_root_token\tcall_output_root_token\texact_cell_reference");
        out.newLine();
    }

    private void row(BufferedWriter out, String sourceId, String callIndex, Function function, Address targetCell,
            String rowType, PcodeOp op, String valueRoot, Function target, String arg0Root,
            String outputRoot, boolean exactReference) throws Exception {
        SequenceNumber seq = op == null ? null : op.getSeqnum();
        out.write(String.join("\t",
            FORMAT,
            cell(sourceId),
            cell(callIndex),
            cell(function.getEntryPoint()),
            cell(function.getName(true)),
            cell(targetCell),
            cell(rowType),
            cell(seq == null ? null : seq.getTarget()),
            cell(seq == null ? null : seq.getTime()),
            cell(valueRoot),
            cell(target == null ? null : target.getEntryPoint()),
            cell(target == null ? null : target.getName(true)),
            cell(arg0Root),
            cell(outputRoot),
            exactReference ? "1" : "0"));
        out.newLine();
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 5) {
            throw new IllegalArgumentException(
                "Usage: ExportExactSingletonCellProvenance.java <function-token> <cell-address> <source-id> <call-index> <output.tsv>");
        }
        Function function = resolveFunction(args[0]);
        Address targetCell = toAddr(args[1]);
        String sourceId = args[2];
        String callIndex = args[3];
        File output = new File(args[4]);
        File parent = output.getParentFile();
        if (parent != null) parent.mkdirs();

        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException("Could not open program");
        int calls = 0;
        int exactStores = 0;
        int exactLoads = 0;
        int returns = 0;
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(
                new FileOutputStream(output), StandardCharsets.UTF_8))) {
            header(out);
            DecompileResults results = decompiler.decompileFunction(function, 60, monitor);
            if (results == null || !results.decompileCompleted() || results.getHighFunction() == null) {
                throw new IllegalStateException("Could not decompile " + function.getEntryPoint());
            }
            HighFunction high = results.getHighFunction();
            Iterator<PcodeOpAST> it = high.getPcodeOps();
            while (it.hasNext() && !monitor.isCancelled()) {
                PcodeOpAST op = it.next();
                int code = op.getOpcode();
                if (code == PcodeOp.CALL) {
                    Function target = directTarget(op);
                    String arg0 = op.getNumInputs() >= 2 ? rootToken(op.getInput(1)) : "-";
                    String outputRoot = rootToken(op.getOutput());
                    row(out, sourceId, callIndex, function, targetCell, "call", op, "-", target, arg0, outputRoot, false);
                    calls++;
                } else if (code == PcodeOp.STORE && op.getNumInputs() >= 3 && instructionReferencesCell(op, targetCell)) {
                    row(out, sourceId, callIndex, function, targetCell, "store", op,
                        rootToken(op.getInput(2)), null, "-", "-", true);
                    exactStores++;
                } else if (code == PcodeOp.LOAD && instructionReferencesCell(op, targetCell)) {
                    row(out, sourceId, callIndex, function, targetCell, "load", op,
                        rootToken(op.getOutput()), null, "-", "-", true);
                    exactLoads++;
                } else if (code == PcodeOp.RETURN) {
                    Varnode value = op.getNumInputs() >= 2 ? op.getInput(op.getNumInputs() - 1) : null;
                    row(out, sourceId, callIndex, function, targetCell, "return", op,
                        rootToken(value), null, "-", "-", false);
                    returns++;
                }
            }
        } finally {
            decompiler.dispose();
        }

        println("EXACT_SINGLETON_CELL_FORMAT=" + FORMAT);
        println("EXACT_SINGLETON_CELL_FUNCTION=" + function.getEntryPoint() + " " + function.getName(true));
        println("EXACT_SINGLETON_CELL_ADDRESS=" + targetCell);
        println("EXACT_SINGLETON_CELL_CALLS=" + calls);
        println("EXACT_SINGLETON_CELL_STORES=" + exactStores);
        println("EXACT_SINGLETON_CELL_LOADS=" + exactLoads);
        println("EXACT_SINGLETON_CELL_RETURNS=" + returns);
        println("EXACT_SINGLETON_CELL_OUTPUT=" + output.getAbsolutePath());
        println("EXACT_SINGLETON_CELL_AFFECTS_ROUTING=no");
    }
}
