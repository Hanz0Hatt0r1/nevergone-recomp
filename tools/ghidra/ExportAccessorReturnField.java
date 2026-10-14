import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.Pointer;
import ghidra.program.model.data.TypeDef;
import ghidra.program.model.listing.Function;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.HighSymbol;
import ghidra.program.model.pcode.HighVariable;
import ghidra.program.model.pcode.LocalSymbolMap;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.PcodeOpAST;
import ghidra.program.model.pcode.Varnode;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

// Evidence-only exporter for small accessors that return a field loaded from a
// function parameter (normally `this`) plus a constant offset. It never mutates
// the program and never infers a class from an accessor name.
public class ExportAccessorReturnField extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraAccessorReturnField/1";
    private static final int MAX_TRANSPARENT_DEPTH = 8;

    private static class TypeInfo {
        final int pointerDepth;
        final DataType pointee;
        TypeInfo(int pointerDepth, DataType pointee) {
            this.pointerDepth = pointerDepth;
            this.pointee = pointee;
        }
    }

    private static class AddressExpr {
        final Varnode base;
        final Long offset;
        final String pattern;
        AddressExpr(Varnode base, Long offset, String pattern) {
            this.base = base;
            this.offset = offset;
            this.pattern = pattern;
        }
    }

    private String cell(Object value) {
        if (value == null) return "-";
        return String.valueOf(value).replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private DataType unwrap(DataType original) {
        DataType cursor = original;
        int guard = 0;
        while (cursor instanceof TypeDef && guard++ < 8) {
            DataType next = ((TypeDef)cursor).getBaseDataType();
            if (next == null || next == cursor) break;
            cursor = next;
        }
        return cursor;
    }

    private TypeInfo inspect(DataType original) {
        DataType cursor = unwrap(original);
        int depth = 0;
        DataType pointee = null;
        while (cursor instanceof Pointer) {
            depth++;
            DataType next = unwrap(((Pointer)cursor).getDataType());
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
        int code = op.getOpcode();
        return code == PcodeOp.COPY || code == PcodeOp.CAST || code == PcodeOp.INDIRECT;
    }

    private Varnode stripTransparent(Varnode original) {
        Varnode cursor = original;
        Set<Varnode> seen = new HashSet<Varnode>();
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

    private AddressExpr decodeAddress(Varnode original) {
        Varnode cursor = stripTransparent(original);
        PcodeOp def = cursor == null ? null : cursor.getDef();
        if (def == null) return new AddressExpr(cursor, 0L, "DIRECT");
        int code = def.getOpcode();
        try {
            if (code == PcodeOp.PTRSUB && def.getNumInputs() >= 2 && constantInput(def, 1)) {
                return new AddressExpr(stripTransparent(def.getInput(0)), def.getInput(1).getOffset(), "PTRSUB");
            }
            if (code == PcodeOp.PTRADD && def.getNumInputs() >= 3 && constantInput(def, 1) && constantInput(def, 2)) {
                long offset = Math.multiplyExact(def.getInput(1).getOffset(), def.getInput(2).getOffset());
                return new AddressExpr(stripTransparent(def.getInput(0)), offset, "PTRADD");
            }
            if (code == PcodeOp.INT_ADD && def.getNumInputs() >= 2) {
                if (constantInput(def, 1)) return new AddressExpr(stripTransparent(def.getInput(0)), def.getInput(1).getOffset(), "INT_ADD");
                if (constantInput(def, 0)) return new AddressExpr(stripTransparent(def.getInput(1)), def.getInput(0).getOffset(), "INT_ADD");
            }
            if (code == PcodeOp.INT_SUB && def.getNumInputs() >= 2 && constantInput(def, 1)) {
                return new AddressExpr(stripTransparent(def.getInput(0)), -def.getInput(1).getOffset(), "INT_SUB");
            }
        } catch (ArithmeticException ignored) {
            return new AddressExpr(null, null, "OFFSET_OVERFLOW");
        }
        return new AddressExpr(null, null, def.getMnemonic());
    }

    private int parameterIndex(HighFunction high, Varnode node) {
        if (high == null || node == null || node.getHigh() == null) return -1;
        LocalSymbolMap map = high.getLocalSymbolMap();
        if (map == null) return -1;
        HighVariable target = node.getHigh();
        for (int i = 0; i < map.getNumParams(); i++) {
            HighSymbol symbol = map.getParamSymbol(i);
            if (symbol == null || symbol.getHighVariable() == null) continue;
            HighVariable parameter = symbol.getHighVariable();
            if (parameter == target) return i;
            if (parameter.getRepresentative() != null && target.getRepresentative() != null &&
                    parameter.getRepresentative().equals(target.getRepresentative())) return i;
        }
        return -1;
    }

    private String parameterType(HighFunction high, int index) {
        if (high == null || index < 0) return "-";
        LocalSymbolMap map = high.getLocalSymbolMap();
        if (map == null || index >= map.getNumParams()) return "-";
        HighSymbol symbol = map.getParamSymbol(index);
        if (symbol == null || symbol.getHighVariable() == null || symbol.getHighVariable().getDataType() == null) return "-";
        return cell(symbol.getHighVariable().getDataType().getDisplayName());
    }

    private Function resolveFunction(String token) throws Exception {
        Address address = toAddr(token);
        Function function = currentProgram.getFunctionManager().getFunctionAt(address);
        if (function == null) function = currentProgram.getFunctionManager().getFunctionContaining(address);
        if (function == null) throw new IllegalArgumentException("Function not found: " + token);
        return function;
    }

    private String hexOffset(Long offset) {
        if (offset == null) return "-";
        if (offset.longValue() < 0) return "-0x" + Long.toUnsignedString(-offset.longValue(), 16);
        return "0x" + Long.toUnsignedString(offset.longValue(), 16);
    }

    private void writeHeader(BufferedWriter out) throws Exception {
        out.write("format\tfunction_entry\tfunction_name\tdeclared_return_type\treturn_index\treturn_address\t" +
            "return_value_type\treturn_pointer_depth\treturn_pointee_type\troot_opcode\tload_address_pattern\t" +
            "load_base_parameter_index\tload_base_parameter_type\tload_offset\tstatus");
        out.newLine();
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) throw new IllegalArgumentException(
            "Usage: ExportAccessorReturnField.java <function-entry> <output.tsv>");
        Function function = resolveFunction(args[0]);
        File output = new File(args[1]);
        File parent = output.getParentFile();
        if (parent != null) parent.mkdirs();

        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException("Could not open current program");
        int returns = 0;
        int fieldReturns = 0;
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(output), StandardCharsets.UTF_8))) {
            writeHeader(out);
            DecompileResults results = decompiler.decompileFunction(function, 60, monitor);
            if (results == null || !results.decompileCompleted() || results.getHighFunction() == null) {
                throw new IllegalStateException("Could not decompile " + function.getEntryPoint());
            }
            HighFunction high = results.getHighFunction();
            Iterator<PcodeOpAST> ops = high.getPcodeOps();
            int returnIndex = 0;
            while (ops.hasNext() && !monitor.isCancelled()) {
                PcodeOpAST op = ops.next();
                if (op.getOpcode() != PcodeOp.RETURN) continue;
                returns++;
                Varnode value = op.getNumInputs() >= 2 ? op.getInput(op.getNumInputs() - 1) : null;
                Varnode root = stripTransparent(value);
                DataType valueType = highType(value);
                TypeInfo typeInfo = inspect(valueType);
                String rootOpcode = root == null || root.getDef() == null ? "-" : root.getDef().getMnemonic();
                String pattern = "-";
                int paramIndex = -1;
                String paramType = "-";
                Long offset = null;
                String status = "return_not_load";

                PcodeOp def = root == null ? null : root.getDef();
                if (def != null && def.getOpcode() == PcodeOp.LOAD && def.getNumInputs() >= 2) {
                    AddressExpr decoded = decodeAddress(def.getInput(1));
                    pattern = decoded.pattern;
                    offset = decoded.offset;
                    paramIndex = parameterIndex(high, decoded.base);
                    paramType = parameterType(high, paramIndex);
                    if (decoded.base == null || decoded.offset == null) status = "load_address_not_constant_base_offset";
                    else if (paramIndex < 0) status = "load_base_not_parameter";
                    else {
                        status = "return_load_from_parameter_constant_offset";
                        fieldReturns++;
                    }
                }

                out.write(String.join("\t",
                    FORMAT,
                    cell(function.getEntryPoint()),
                    cell(function.getName()),
                    cell(function.getReturnType() == null ? null : function.getReturnType().getDisplayName()),
                    Integer.toString(returnIndex),
                    cell(op.getSeqnum().getTarget()),
                    cell(valueType == null ? null : valueType.getDisplayName()),
                    Integer.toString(typeInfo.pointerDepth),
                    cell(typeInfo.pointee == null ? null : typeInfo.pointee.getDisplayName()),
                    rootOpcode,
                    pattern,
                    paramIndex >= 0 ? Integer.toString(paramIndex) : "-",
                    paramType,
                    hexOffset(offset),
                    status));
                out.newLine();
                returnIndex++;
            }
        } finally {
            decompiler.dispose();
        }

        println("ACCESSOR_RETURN_FIELD_FORMAT=" + FORMAT);
        println("ACCESSOR_RETURN_FIELD_FUNCTION=" + function.getEntryPoint() + " " + function.getName());
        println("ACCESSOR_RETURN_FIELD_RETURNS=" + returns);
        println("ACCESSOR_RETURN_FIELD_CONSTANT_FIELD_RETURNS=" + fieldReturns);
        println("ACCESSOR_RETURN_FIELD_OUTPUT=" + output.getAbsolutePath());
        println("ACCESSOR_RETURN_FIELD_AFFECTS_ROUTING=no");
    }
}
