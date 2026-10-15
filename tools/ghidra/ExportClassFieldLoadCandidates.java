import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.PcodeOpAST;
import ghidra.program.model.pcode.Varnode;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.util.Iterator;

// Evidence-only inventory of direct field loads in named classes. A matching
// offset alone does not establish the base object's dynamic type or a call path.
public class ExportClassFieldLoadCandidates extends GhidraScript {
    private String cell(Object value) {
        return String.valueOf(value).replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private String token(Varnode node) {
        if (node == null) return "-";
        if (node.isConstant()) return "const:0x" + Long.toUnsignedString(node.getOffset(), 16) + ":" + node.getSize();
        if (node.getDef() != null) {
            PcodeOp definition = node.getDef();
            return "def:" + cell(definition.getSeqnum().getTarget()) + ":" + definition.getSeqnum().getTime() + ":" + node.getSize();
        }
        return (node.isRegister() ? "register:" : node.isUnique() ? "unique:" : "input:") +
            cell(node.getAddress()) + ":" + node.getSize();
    }

    private Varnode strip(Varnode node) {
        for (int i = 0; node != null && i < 8; i++) {
            PcodeOp definition = node.getDef();
            if (definition == null || definition.getNumInputs() == 0) break;
            int code = definition.getOpcode();
            if (code != PcodeOp.COPY && code != PcodeOp.CAST && code != PcodeOp.INDIRECT) break;
            node = definition.getInput(0);
        }
        return node;
    }

    private Varnode baseForOffset(Varnode address, long offset) {
        Varnode cursor = strip(address);
        PcodeOp definition = cursor == null ? null : cursor.getDef();
        if (definition == null) return null;
        int code = definition.getOpcode();
        if (code == PcodeOp.PTRADD && definition.getNumInputs() >= 3 &&
                definition.getInput(1).isConstant() && definition.getInput(2).isConstant() &&
                definition.getInput(1).getOffset() == offset && definition.getInput(2).getOffset() == 1) {
            return strip(definition.getInput(0));
        }
        if (code == PcodeOp.PTRSUB && definition.getNumInputs() >= 2 &&
                definition.getInput(1).isConstant() && definition.getInput(1).getOffset() == offset) {
            return strip(definition.getInput(0));
        }
        if (code == PcodeOp.INT_ADD && definition.getNumInputs() >= 2) {
            if (definition.getInput(1).isConstant() && definition.getInput(1).getOffset() == offset)
                return strip(definition.getInput(0));
            if (definition.getInput(0).isConstant() && definition.getInput(0).getOffset() == offset)
                return strip(definition.getInput(1));
        }
        return null;
    }

    private boolean matches(Function function, String[] tokens) {
        String name = function.getName(true);
        for (String token : tokens) if (!token.isEmpty() && name.contains(token)) return true;
        return false;
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 3) throw new IllegalArgumentException(
            "Usage: ExportClassFieldLoadCandidates.java <name-token;...> <offset> <output.tsv>");
        String[] names = args[0].split(";");
        long offset = Long.decode(args[1]);
        File output = new File(args[2]);
        if (output.getParentFile() != null) output.getParentFile().mkdirs();
        long scanned = 0;
        long loads = 0;
        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException("Could not open program");
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(
                new FileOutputStream(output), StandardCharsets.UTF_8))) {
            out.write("function_entry\tfunction_name\tload_address\tload_time\tbase_token\tvalue_token");
            out.newLine();
            FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
            while (functions.hasNext() && !monitor.isCancelled()) {
                Function function = functions.next();
                if (!matches(function, names)) continue;
                scanned++;
                DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
                if (result == null || !result.decompileCompleted()) continue;
                HighFunction high = result.getHighFunction();
                if (high == null) continue;
                Iterator<PcodeOpAST> operations = high.getPcodeOps();
                while (operations.hasNext() && !monitor.isCancelled()) {
                    PcodeOpAST op = operations.next();
                    if (op.getOpcode() != PcodeOp.LOAD || op.getNumInputs() < 2) continue;
                    Varnode base = baseForOffset(op.getInput(1), offset);
                    if (base == null) continue;
                    out.write(cell(function.getEntryPoint()) + "\t" + cell(function.getName(true)) + "\t" +
                        cell(op.getSeqnum().getTarget()) + "\t" + op.getSeqnum().getTime() + "\t" +
                        token(base) + "\t" + token(op.getOutput()));
                    out.newLine();
                    loads++;
                }
            }
        } finally {
            decompiler.dispose();
        }
        println("Scanned " + scanned + " matching functions; exported " + loads +
            " loads at +0x" + Long.toUnsignedString(offset, 16) + " -> " + output);
    }
}
