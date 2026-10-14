import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Symbol;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;

// Evidence-only metadata probe for suspicious CALLIND target addresses.
// Exports section/symbol/function/thunk/listing metadata and a 32-bit raw word.
// It does not modify the program and does not infer routing.
public class ExportAddressClassification extends GhidraScript {
    private String cell(Object value) {
        if (value == null) return "-";
        return String.valueOf(value).replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private String functionName(Function function) {
        return function == null ? "-" : function.getName(true);
    }

    private String functionEntry(Function function) {
        return function == null ? "-" : cell(function.getEntryPoint());
    }

    private String functionBody(Function function) {
        if (function == null || function.getBody() == null || function.getBody().isEmpty()) return "-";
        return cell(function.getBody().getMinAddress()) + ".." + cell(function.getBody().getMaxAddress());
    }

    private String thunkTarget(Function function) {
        if (function == null || !function.isThunk()) return "-";
        try {
            Function target = function.getThunkedFunction(true);
            if (target == null) return "-";
            return cell(target.getEntryPoint()) + ":" + cell(target.getName(true));
        } catch (Exception ignored) {
            return "<error>";
        }
    }

    private String rawWord(Address address) {
        try {
            byte[] bytes = new byte[4];
            int got = currentProgram.getMemory().getBytes(address, bytes);
            if (got != 4) return "-";
            long value;
            if (currentProgram.getLanguage().isBigEndian()) {
                value = ((bytes[0] & 0xffL) << 24) | ((bytes[1] & 0xffL) << 16) |
                    ((bytes[2] & 0xffL) << 8) | (bytes[3] & 0xffL);
            } else {
                value = (bytes[0] & 0xffL) | ((bytes[1] & 0xffL) << 8) |
                    ((bytes[2] & 0xffL) << 16) | ((bytes[3] & 0xffL) << 24);
            }
            return "0x" + Long.toUnsignedString(value, 16);
        } catch (Exception ignored) {
            return "-";
        }
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            throw new IllegalArgumentException("Usage: ExportAddressClassification.java <output.tsv> <address> [address ...]");
        }

        File output = new File(args[0]);
        File parent = output.getParentFile();
        if (parent != null) parent.mkdirs();

        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(
                new FileOutputStream(output), StandardCharsets.UTF_8))) {
            out.write("format\taddress\tblock\tblock_start\tblock_end\tread\twrite\texecute\tprimary_symbol\tfunction_at_entry\tfunction_at_name\tfunction_at_body\tfunction_at_is_thunk\tfunction_at_thunk_target\tfunction_containing_entry\tfunction_containing_name\tfunction_containing_body\tinstruction_mnemonic\tdata_type\traw_u32");
            out.newLine();

            for (int i = 1; i < args.length; i++) {
                Address address = toAddr(args[i]);
                MemoryBlock block = currentProgram.getMemory().getBlock(address);
                Symbol symbol = currentProgram.getSymbolTable().getPrimarySymbol(address);
                Function at = currentProgram.getFunctionManager().getFunctionAt(address);
                Function containing = currentProgram.getFunctionManager().getFunctionContaining(address);
                Instruction instruction = currentProgram.getListing().getInstructionAt(address);
                Data data = currentProgram.getListing().getDataAt(address);

                out.write("NEVERGONE.GhidraAddressClassification/1\t" + cell(address) + "\t" +
                    cell(block == null ? null : block.getName()) + "\t" +
                    cell(block == null ? null : block.getStart()) + "\t" +
                    cell(block == null ? null : block.getEnd()) + "\t" +
                    (block == null ? "-" : (block.isRead() ? "1" : "0")) + "\t" +
                    (block == null ? "-" : (block.isWrite() ? "1" : "0")) + "\t" +
                    (block == null ? "-" : (block.isExecute() ? "1" : "0")) + "\t" +
                    cell(symbol == null ? null : symbol.getName()) + "\t" +
                    functionEntry(at) + "\t" + functionName(at) + "\t" + functionBody(at) + "\t" +
                    (at == null ? "-" : (at.isThunk() ? "1" : "0")) + "\t" + thunkTarget(at) + "\t" +
                    functionEntry(containing) + "\t" + functionName(containing) + "\t" + functionBody(containing) + "\t" +
                    cell(instruction == null ? null : instruction.getMnemonicString()) + "\t" +
                    cell(data == null || data.getDataType() == null ? null : data.getDataType().getName()) + "\t" +
                    rawWord(address));
                out.newLine();
            }
        }

        println("ADDRESS_CLASSIFICATION_FORMAT=NEVERGONE.GhidraAddressClassification/1");
        println("ADDRESS_CLASSIFICATION_ROWS=" + (args.length - 1));
        println("ADDRESS_CLASSIFICATION_OUTPUT=" + output.getAbsolutePath());
        println("ADDRESS_CLASSIFICATION_AFFECTS_ROUTING=no");
    }
}
