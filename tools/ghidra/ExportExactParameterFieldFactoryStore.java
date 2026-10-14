import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.HighSymbol;
import ghidra.program.model.pcode.HighVariable;
import ghidra.program.model.pcode.LocalSymbolMap;
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

// Evidence-only exporter for STOREs to parameter-relative fields in one exact
// function. Stored values are stripped only through COPY/CAST/INDIRECT. If the
// resulting root is a direct CALL, the exact target is emitted. No class or
// routing inference is performed here.
public class ExportExactParameterFieldFactoryStore extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraExactParameterFieldFactoryStore/1";
    private static final int MAX_DEPTH = 16;
    private static final int MAX_TRANSPARENT = 8;

    private static class Expr {
        final Varnode base;
        final Long offset;
        final String kind;
        Expr(Varnode base, Long offset, String kind) {
            this.base = base; this.offset = offset; this.kind = kind;
        }
    }

    private String cell(Object value) {
        if (value == null) return "-";
        return String.valueOf(value).replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private String token(Varnode node) {
        if (node == null) return "-";
        if (node.isConstant())
            return "const:0x" + Long.toUnsignedString(node.getOffset(), 16) + ":" + node.getSize();
        PcodeOp def = node.getDef();
        if (def != null) {
            SequenceNumber seq = def.getSeqnum();
            return "def:" + def.getMnemonic() + "@" + cell(seq.getTarget()) + ":" + seq.getTime() + ":" + node.getSize();
        }
        String kind = node.isRegister() ? "register" : node.isUnique() ? "unique" :
            node.isAddress() ? "address" : "input";
        return kind + ":" + cell(node.getAddress()) + ":" + node.getSize();
    }

    private boolean transparent(PcodeOp op) {
        if (op == null) return false;
        int code = op.getOpcode();
        return code == PcodeOp.COPY || code == PcodeOp.CAST || code == PcodeOp.INDIRECT;
    }

    private Varnode strip(Varnode original) {
        Varnode cursor = original;
        HashSet<Varnode> seen = new HashSet<Varnode>();
        for (int depth = 0; cursor != null && depth < MAX_TRANSPARENT && seen.add(cursor); depth++) {
            PcodeOp def = cursor.getDef();
            if (!transparent(def) || def.getNumInputs() < 1) break;
            cursor = def.getInput(0);
        }
        return cursor;
    }

    private Expr decode(Varnode node, int depth, Set<Varnode> active) {
        if (node == null || depth > MAX_DEPTH || active.contains(node)) return new Expr(null, null, "missing_or_cycle");
        if (node.isConstant()) return new Expr(null, node.getOffset(), "constant");
        PcodeOp def = node.getDef();
        if (def == null) return new Expr(node, 0L, "base");
        Set<Varnode> next = new HashSet<Varnode>(active); next.add(node);
        if (transparent(def) && def.getNumInputs() >= 1) return decode(def.getInput(0), depth + 1, next);
        int code = def.getOpcode();
        if (code == PcodeOp.MULTIEQUAL) return new Expr(null, null, "phi_ambiguous");
        try {
            if (def.getNumInputs() >= 2 && (code == PcodeOp.PTRSUB || code == PcodeOp.INT_ADD || code == PcodeOp.INT_SUB || code == PcodeOp.PTRADD)) {
                Varnode a = def.getInput(0), b = def.getInput(1), base = a;
                Long delta = null;
                if (b != null && b.isConstant()) {
                    delta = b.getOffset();
                    if (code == PcodeOp.INT_SUB) delta = Math.negateExact(delta);
                    if (code == PcodeOp.PTRADD) {
                        if (def.getNumInputs() < 3 || !def.getInput(2).isConstant()) return new Expr(null, null, "ptradd_unknown_scale");
                        delta = Math.multiplyExact(delta, def.getInput(2).getOffset());
                    }
                } else if (code == PcodeOp.INT_ADD && a != null && a.isConstant()) {
                    delta = a.getOffset(); base = b;
                }
                if (delta != null) {
                    Expr child = decode(base, depth + 1, next);
                    if (child.offset == null) return new Expr(null, null, "unresolved_arithmetic_base");
                    return new Expr(child.base, Math.addExact(child.offset, delta), "base_plus_constant");
                }
            }
        } catch (ArithmeticException ignored) {
            return new Expr(null, null, "offset_overflow");
        }
        return new Expr(node, 0L, "opaque_" + def.getMnemonic());
    }

    private int parameterIndex(HighFunction high, Varnode node) {
        if (high == null || node == null || node.getHigh() == null) return -1;
        LocalSymbolMap map = high.getLocalSymbolMap();
        if (map == null) return -1;
        HighVariable candidate = node.getHigh();
        for (int i = 0; i < map.getNumParams(); i++) {
            HighSymbol symbol = map.getParamSymbol(i);
            if (symbol == null || symbol.getHighVariable() == null) continue;
            HighVariable param = symbol.getHighVariable();
            if (candidate == param) return i;
            if (candidate.getRepresentative() != null && param.getRepresentative() != null &&
                candidate.getRepresentative().equals(param.getRepresentative())) return i;
        }
        return -1;
    }

    private Function directTarget(PcodeOp op) {
        if (op == null || op.getOpcode() != PcodeOp.CALL || op.getNumInputs() < 1) return null;
        Varnode target = strip(op.getInput(0));
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

    private String hex(Long value) {
        if (value == null) return "-";
        if (value.longValue() < 0) return "-0x" + Long.toUnsignedString(-value.longValue(), 16);
        return "0x" + Long.toUnsignedString(value.longValue(), 16);
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) throw new IllegalArgumentException("Usage: ExportExactParameterFieldFactoryStore.java <function-entry> <output.tsv>");
        Address entry = toAddr(args[0]);
        Function function = currentProgram.getFunctionManager().getFunctionAt(entry);
        if (function == null) throw new IllegalArgumentException("Function not found at exact entry: " + args[0]);
        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException("Could not open program");
        DecompileResults results = decompiler.decompileFunction(function, 60, monitor);
        if (results == null || !results.decompileCompleted() || results.getHighFunction() == null)
            throw new IllegalStateException("Could not decompile " + args[0]);
        HighFunction high = results.getHighFunction();
        File output = new File(args[1]);
        if (output.getParentFile() != null) output.getParentFile().mkdirs();
        int stores = 0;
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(output), StandardCharsets.UTF_8))) {
            out.write("format\tfunction_entry\tfunction_name\top_address\top_time\taddress_kind\taddress_base_token\taddress_offset\tbase_parameter_index\tvalue_token\tvalue_root_token\tvalue_root_opcode\tfactory_target_entry\tfactory_target_name");
            out.newLine();
            Iterator<PcodeOpAST> it = high.getPcodeOps();
            while (it.hasNext()) {
                PcodeOpAST op = it.next();
                if (op.getOpcode() != PcodeOp.STORE || op.getNumInputs() < 3) continue;
                Expr expr = decode(op.getInput(1), 0, new HashSet<Varnode>());
                int param = parameterIndex(high, expr.base);
                Varnode value = op.getInput(2);
                Varnode root = strip(value);
                PcodeOp rootDef = root == null ? null : root.getDef();
                Function target = directTarget(rootDef);
                SequenceNumber seq = op.getSeqnum();
                out.write(String.join("\t",
                    FORMAT, cell(function.getEntryPoint()), cell(function.getName(true)),
                    cell(seq == null ? null : seq.getTarget()), cell(seq == null ? null : seq.getTime()),
                    cell(expr.kind), token(expr.base), hex(expr.offset), param < 0 ? "-" : Integer.toString(param),
                    token(value), token(root), cell(rootDef == null ? null : rootDef.getMnemonic()),
                    cell(target == null ? null : target.getEntryPoint()), cell(target == null ? null : target.getName(true))));
                out.newLine(); stores++;
            }
        } finally {
            decompiler.dispose();
        }
        println("EXACT_PARAMETER_FIELD_FACTORY_STORE_FORMAT=" + FORMAT);
        println("EXACT_PARAMETER_FIELD_FACTORY_STORE_FUNCTION=" + function.getEntryPoint() + ":" + function.getName(true));
        println("EXACT_PARAMETER_FIELD_FACTORY_STORE_ROWS=" + stores);
        println("EXACT_PARAMETER_FIELD_FACTORY_STORE_OUTPUT=" + output.getAbsolutePath());
        println("EXACT_PARAMETER_FIELD_FACTORY_STORE_AFFECTS_ROUTING=no");
    }
}
