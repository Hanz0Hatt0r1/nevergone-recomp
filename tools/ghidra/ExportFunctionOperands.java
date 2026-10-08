import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.util.HashSet;
import java.util.Set;

// Focused clean-room metadata export for one analyzed function. The output
// contains numeric operands and referenced scalar data only; it does not export
// instruction bytes, disassembly text, decompiler output, or source assets.
public class ExportFunctionOperands extends GhidraScript {
    private String cell(Object value) {
        return String.valueOf(value).replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private Function resolveFunction(String token) throws Exception {
        Address address = null;
        try {
            address = toAddr(token);
        } catch (Exception ignored) {
            // Fall through to a name lookup.
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

    private String scalarUnsignedHex(Scalar scalar) {
        return "0x" + Long.toUnsignedString(scalar.getUnsignedValue(), 16);
    }

    private void writeScalar(
            BufferedWriter out,
            Function function,
            Address instructionAddress,
            int operandIndex,
            String kind,
            Scalar scalar,
            Address targetAddress,
            String targetType) throws Exception {
        out.write(cell(function.getEntryPoint()) + "\t" + cell(instructionAddress) + "\t" +
            operandIndex + "\t" + kind + "\t" + scalar.getBitLength() + "\t" +
            scalar.getSignedValue() + "\t" + scalarUnsignedHex(scalar) + "\t" +
            (targetAddress == null ? "-" : cell(targetAddress)) + "\t" +
            (targetType == null ? "-" : cell(targetType)) + "\t-");
        out.newLine();
    }

    private void writeNumber(
            BufferedWriter out,
            Function function,
            Address instructionAddress,
            int operandIndex,
            String kind,
            Number value,
            Address targetAddress,
            String targetType) throws Exception {
        out.write(cell(function.getEntryPoint()) + "\t" + cell(instructionAddress) + "\t" +
            operandIndex + "\t" + kind + "\t-\t-\t-\t" +
            (targetAddress == null ? "-" : cell(targetAddress)) + "\t" +
            (targetType == null ? "-" : cell(targetType)) + "\t" + cell(value));
        out.newLine();
    }

    private boolean writeReferencedData(
            BufferedWriter out,
            Function function,
            Address instructionAddress,
            int operandIndex,
            Address targetAddress,
            Set<String> seen) throws Exception {
        if (targetAddress == null) return false;
        Data data = currentProgram.getListing().getDataAt(targetAddress);
        if (data == null) return false;
        Object value = data.getValue();
        String key = instructionAddress + ":" + operandIndex + ":" + targetAddress;
        if (!seen.add(key)) return true;
        String dataType = data.getDataType().getName();
        if (value instanceof Scalar) {
            writeScalar(out, function, instructionAddress, operandIndex,
                "referenced-scalar", (Scalar)value, targetAddress, dataType);
            return true;
        }
        if (value instanceof Number) {
            writeNumber(out, function, instructionAddress, operandIndex,
                "referenced-number", (Number)value, targetAddress, dataType);
            return true;
        }
        return false;
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException(
                "Usage: ExportFunctionOperands.java <function-address-or-name> <output.tsv>");
        }
        Function function = resolveFunction(args[0]);
        File output = new File(args[1]);
        File parent = output.getParentFile();
        if (parent != null) parent.mkdirs();

        long rows = 0;
        Set<String> seenReferencedData = new HashSet<>();
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(
                new FileOutputStream(output), StandardCharsets.UTF_8))) {
            out.write("function_entry\tinstruction\toperand\tkind\tbit_length\tsigned_value\tunsigned_hex\ttarget\ttarget_type\tnumber_value");
            out.newLine();

            InstructionIterator instructions = currentProgram.getListing()
                .getInstructions(function.getBody(), true);
            while (instructions.hasNext() && !monitor.isCancelled()) {
                Instruction instruction = instructions.next();
                for (int operandIndex = 0; operandIndex < instruction.getNumOperands(); operandIndex++) {
                    Object[] objects = instruction.getOpObjects(operandIndex);
                    for (Object object : objects) {
                        if (object instanceof Scalar) {
                            writeScalar(out, function, instruction.getAddress(), operandIndex,
                                "immediate", (Scalar)object, null, null);
                            rows++;
                        } else if (object instanceof Address && writeReferencedData(
                                out, function, instruction.getAddress(), operandIndex,
                                (Address)object, seenReferencedData)) {
                            rows++;
                        }
                    }
                }

                for (Reference reference : instruction.getReferencesFrom()) {
                    if (reference.getReferenceType().isCall()) continue;
                    if (writeReferencedData(
                            out,
                            function,
                            instruction.getAddress(),
                            reference.getOperandIndex(),
                            reference.getToAddress(),
                            seenReferencedData)) {
                        rows++;
                    }
                }
            }
        }
        println("Exported " + rows + " numeric operand/data rows for " +
            function.getName(true) + " at " + function.getEntryPoint() + " -> " + output);
    }
}
