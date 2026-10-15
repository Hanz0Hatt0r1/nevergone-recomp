import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.PcodeOpAST;
import ghidra.program.model.pcode.Varnode;
import ghidra.program.model.scalar.Scalar;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

// Evidence-only locator for stripped EnemyObject::loadEnemyObjectFile-like
// functions. It does not rely on symbols. The listing pass first selects only
// functions whose instructions contain both recovered object offsets 0x160 and
// 0x164 as scalar operands. High P-code then confirms STORE destinations that
// resolve through bounded address expressions to each offset.
//
// Output contains only derived function/store counts. It intentionally omits
// instruction bytes, disassembly text, decompiler C text, strings, and assets.
public class ExportEnemyHealthFunctionCandidates extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.EnemyHealthFunctionCandidates/1";
    private static final long MAX_HEALTH_OFFSET = 0x160L;
    private static final long CURRENT_HEALTH_OFFSET = 0x164L;
    private static final int MAX_ADDRESS_DEPTH = 32;

    private String cell(Object value) {
        if (value == null) return "-";
        return String.valueOf(value).replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private boolean instructionHasScalar(Instruction instruction, long target) {
        for (int operand = 0; operand < instruction.getNumOperands(); ++operand) {
            Object[] objects = instruction.getOpObjects(operand);
            if (objects == null) continue;
            for (Object object : objects) {
                if (!(object instanceof Scalar)) continue;
                Scalar scalar = (Scalar) object;
                if (scalar.getUnsignedValue() == target) return true;
            }
        }
        return false;
    }

    private boolean listingPrefilter(Function function) {
        Listing listing = currentProgram.getListing();
        InstructionIterator instructions = listing.getInstructions(function.getBody(), true);
        boolean sawMax = false;
        boolean sawCurrent = false;
        while (instructions.hasNext() && !monitor.isCancelled()) {
            Instruction instruction = instructions.next();
            if (!sawMax && instructionHasScalar(instruction, MAX_HEALTH_OFFSET)) {
                sawMax = true;
            }
            if (!sawCurrent && instructionHasScalar(instruction, CURRENT_HEALTH_OFFSET)) {
                sawCurrent = true;
            }
            if (sawMax && sawCurrent) return true;
        }
        return false;
    }

    private boolean containsFieldOffset(
            Varnode node,
            long fieldOffset,
            int depth,
            Set<Varnode> active) {
        if (node == null || depth > MAX_ADDRESS_DEPTH || active.contains(node)) return false;
        PcodeOp definition = node.getDef();
        if (definition == null) return false;

        int opcode = definition.getOpcode();
        if (opcode == PcodeOp.PTRSUB || opcode == PcodeOp.INT_ADD ||
                opcode == PcodeOp.INT_SUB || opcode == PcodeOp.PTRADD) {
            for (int index = 0; index < definition.getNumInputs(); ++index) {
                Varnode input = definition.getInput(index);
                if (input != null && input.isConstant() && input.getOffset() == fieldOffset) {
                    return true;
                }
            }
        }

        Set<Varnode> next = new HashSet<Varnode>(active);
        next.add(node);
        for (int index = 0; index < definition.getNumInputs(); ++index) {
            Varnode input = definition.getInput(index);
            if (input == null || input.isConstant()) continue;
            if (containsFieldOffset(input, fieldOffset, depth + 1, next)) return true;
        }
        return false;
    }

    private int countStores(HighFunction high, long fieldOffset) {
        int count = 0;
        Iterator<PcodeOpAST> operations = high.getPcodeOps();
        while (operations.hasNext() && !monitor.isCancelled()) {
            PcodeOpAST operation = operations.next();
            if (operation.getOpcode() != PcodeOp.STORE || operation.getNumInputs() < 3) continue;
            if (containsFieldOffset(
                    operation.getInput(1),
                    fieldOffset,
                    0,
                    new HashSet<Varnode>())) {
                count++;
            }
        }
        return count;
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException(
                "Usage: ExportEnemyHealthFunctionCandidates.java <output.tsv>");
        }

        File output = new File(args[0]);
        File parent = output.getParentFile();
        if (parent != null) parent.mkdirs();

        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) {
            throw new IllegalStateException("Could not open current program in decompiler");
        }

        long functionsSeen = 0;
        long functionsPrefiltered = 0;
        long candidates = 0;
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(
                new FileOutputStream(output), StandardCharsets.UTF_8))) {
            out.write(
                "format\tfunction_entry\tfunction_name\tstores_0x160\tstores_0x164");
            out.newLine();

            FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
            while (functions.hasNext() && !monitor.isCancelled()) {
                Function function = functions.next();
                functionsSeen++;
                if (!listingPrefilter(function)) continue;
                functionsPrefiltered++;

                DecompileResults results = decompiler.decompileFunction(function, 60, monitor);
                if (results == null || !results.decompileCompleted() ||
                        results.getHighFunction() == null) {
                    continue;
                }

                HighFunction high = results.getHighFunction();
                int maxStores = countStores(high, MAX_HEALTH_OFFSET);
                if (maxStores == 0) continue;
                int currentStores = countStores(high, CURRENT_HEALTH_OFFSET);
                if (currentStores == 0) continue;

                out.write(String.join("\t",
                    FORMAT,
                    cell(function.getEntryPoint()),
                    cell(function.getName(true)),
                    Integer.toString(maxStores),
                    Integer.toString(currentStores)));
                out.newLine();
                candidates++;
            }
        } finally {
            decompiler.dispose();
        }

        println("ENEMY_HEALTH_FUNCTION_LOCATOR_FORMAT=" + FORMAT);
        println("ENEMY_HEALTH_FUNCTION_LOCATOR_FUNCTIONS_SEEN=" + functionsSeen);
        println("ENEMY_HEALTH_FUNCTION_LOCATOR_PREFILTERED=" + functionsPrefiltered);
        println("ENEMY_HEALTH_FUNCTION_LOCATOR_CANDIDATES=" + candidates);
        println("ENEMY_HEALTH_FUNCTION_LOCATOR_OUTPUT=" + output.getAbsolutePath());
        println("ENEMY_HEALTH_FUNCTION_LOCATOR_SEMANTIC_CLAIM=none");
    }
}
