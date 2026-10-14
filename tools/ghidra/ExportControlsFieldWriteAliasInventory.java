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

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

// Read-only, exact-entry high-P-code inventory of EVERY STORE and CALL in the
// original Controls methods. A STORE offset by itself never proves the base
// object is Controls; indirect calls and aliases remain unresolved.
public class ExportControlsFieldWriteAliasInventory extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraControlsFieldWriteAliasInventory/1";
    private static final int MAX_DEPTH = 16;

    private static class AddressExpr {
        final Varnode base;
        final Long offset;
        final String kind;
        AddressExpr(Varnode base, Long offset, String kind) {
            this.base = base;
            this.offset = offset;
            this.kind = kind;
        }
    }

    private String cell(Object value) {
        if (value == null) return "-";
        return String.valueOf(value).replace('\t', ' ').replace('\r', ' ').replace('\n', ' ');
    }

    private String token(Varnode node) {
        if (node == null) return "-";
        if (node.isConstant())
            return "const:0x" + Long.toUnsignedString(node.getOffset(), 16) + ":" + node.getSize();
        PcodeOp def = node.getDef();
        if (def != null) {
            SequenceNumber seq = def.getSeqnum();
            return "def:" + def.getMnemonic() + "@" + cell(seq.getTarget()) + ":" +
                seq.getTime() + ":" + node.getSize();
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

    private AddressExpr decode(Varnode node, int depth, Set<Varnode> active) {
        if (node == null || depth > MAX_DEPTH || active.contains(node))
            return new AddressExpr(null, null, "missing_or_cycle");
        if (node.isConstant()) return new AddressExpr(null, node.getOffset(), "constant");
        PcodeOp def = node.getDef();
        if (def == null) return new AddressExpr(node, 0L, "base");
        Set<Varnode> next = new HashSet<Varnode>(active);
        next.add(node);
        if (transparent(def) && def.getNumInputs() >= 1)
            return decode(def.getInput(0), depth + 1, next);
        int code = def.getOpcode();
        if (code == PcodeOp.MULTIEQUAL)
            return new AddressExpr(null, null, "phi_ambiguous");
        try {
            if (def.getNumInputs() >= 2 &&
                (code == PcodeOp.PTRSUB || code == PcodeOp.INT_ADD ||
                 code == PcodeOp.INT_SUB || code == PcodeOp.PTRADD)) {
                Varnode a = def.getInput(0);
                Varnode b = def.getInput(1);
                Long delta = null;
                Varnode base = a;
                if (b != null && b.isConstant()) {
                    delta = b.getOffset();
                    if (code == PcodeOp.INT_SUB) delta = Math.negateExact(delta);
                    if (code == PcodeOp.PTRADD) {
                        if (def.getNumInputs() < 3 || !def.getInput(2).isConstant())
                            return new AddressExpr(null, null, "ptradd_unknown_scale");
                        delta = Math.multiplyExact(delta, def.getInput(2).getOffset());
                    }
                } else if (code == PcodeOp.INT_ADD && a != null && a.isConstant()) {
                    delta = a.getOffset();
                    base = b;
                }
                if (delta != null) {
                    AddressExpr child = decode(base, depth + 1, next);
                    if (child.offset == null)
                        return new AddressExpr(null, null, "unresolved_arithmetic_base");
                    return new AddressExpr(child.base, Math.addExact(child.offset, delta),
                        "base_plus_constant");
                }
            }
        } catch (ArithmeticException ignored) {
            return new AddressExpr(null, null, "offset_overflow");
        }
        // Opaque address expressions cannot be silently equated to this+field.
        return new AddressExpr(node, 0L, "opaque_" + def.getMnemonic());
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

    private String hex(Long value) {
        if (value == null) return "-";
        return value.longValue() < 0
            ? "-0x" + Long.toUnsignedString(-value.longValue(), 16)
            : "0x" + Long.toUnsignedString(value.longValue(), 16);
    }

    private void header(BufferedWriter out) throws Exception {
        out.write("format\trow_type\tfunction_entry\tfunction_name\top_address\top_time\t" +
            "decompile_status\taddress_kind\taddress_base_token\taddress_offset\t" +
            "base_parameter_index\tvalue_token\tvalue_defining_opcode\tcall_kind\t" +
            "call_target_token\tthis_argument_indices\tstore_count\tcall_count\t" +
            "unknown_store_count\tthis_escape_count");
        out.newLine();
    }

    private void write(BufferedWriter out, String type, Function function, String entry,
            PcodeOp op, String status, AddressExpr expr, int param, Varnode value,
            String callKind, String target, String thisArgs,
            int stores, int calls, int unknown, int escapes) throws Exception {
        SequenceNumber seq = op == null ? null : op.getSeqnum();
        PcodeOp def = value == null ? null : value.getDef();
        out.write(String.join("\t",
            FORMAT, type, entry, cell(function == null ? null : function.getName(true)),
            cell(seq == null ? null : seq.getTarget()),
            cell(seq == null ? null : seq.getTime()), status,
            expr == null ? "-" : cell(expr.kind),
            expr == null ? "-" : token(expr.base),
            expr == null ? "-" : hex(expr.offset),
            param < 0 ? "-" : Integer.toString(param),
            token(value), cell(def == null ? null : def.getMnemonic()),
            cell(callKind), cell(target), cell(thisArgs),
            Integer.toString(stores), Integer.toString(calls),
            Integer.toString(unknown), Integer.toString(escapes)));
        out.newLine();
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) throw new IllegalArgumentException(
            "Usage: ExportControlsFieldWriteAliasInventory.java <methods.tsv> <output.tsv>");
        File output = new File(args[1]);
        if (output.getParentFile() != null) output.getParentFile().mkdirs();
        List<String> entries = new ArrayList<String>();
        Set<String> unique = new HashSet<String>();
        try (BufferedReader input = Files.newBufferedReader(
                Paths.get(args[0]), StandardCharsets.UTF_8)) {
            String header = input.readLine();
            if (!"function_entry\telf_entry\tsymbol_names".equals(header))
                throw new IllegalArgumentException("invalid Controls method manifest header");
            String line;
            while ((line = input.readLine()) != null) {
                if (line.trim().isEmpty()) continue;
                String[] cells = line.split("\t", -1);
                if (cells.length != 3 || !cells[0].matches("[0-9a-f]{8}") ||
                    !unique.add(cells[0]))
                    throw new IllegalArgumentException("invalid or duplicate method entry: " + line);
                entries.add(cells[0]);
            }
        }
        if (entries.size() != 38)
            throw new IllegalArgumentException("expected 38 unique Controls method entries");

        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram))
            throw new IllegalStateException("could not open program");
        int completed = 0;
        int failed = 0;
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(
                new FileOutputStream(output), StandardCharsets.UTF_8))) {
            header(out);
            for (String entry : entries) {
                if (monitor.isCancelled()) throw new IllegalStateException("Ghidra scan cancelled");
                Address address = toAddr(entry);
                Function function = currentProgram.getFunctionManager().getFunctionAt(address);
                if (function == null) {
                    write(out, "summary", null, entry, null, "missing_function",
                        null, -1, null, "-", "-", "-", 0, 0, 0, 0);
                    failed++;
                    continue;
                }
                DecompileResults results = decompiler.decompileFunction(function, 60, monitor);
                if (results == null || !results.decompileCompleted() ||
                    results.getHighFunction() == null) {
                    write(out, "summary", function, entry, null, "decompile_failed",
                        null, -1, null, "-", "-", "-", 0, 0, 0, 0);
                    failed++;
                    continue;
                }
                HighFunction high = results.getHighFunction();
                int stores = 0;
                int calls = 0;
                int unknown = 0;
                int escapes = 0;
                Iterator<PcodeOpAST> it = high.getPcodeOps();
                while (it.hasNext()) {
                    if (monitor.isCancelled()) throw new IllegalStateException("Ghidra scan cancelled");
                    PcodeOpAST op = it.next();
                    int code = op.getOpcode();
                    if (code == PcodeOp.STORE && op.getNumInputs() >= 3) {
                        AddressExpr expr = decode(op.getInput(1), 0, new HashSet<Varnode>());
                        int param = parameterIndex(high, expr.base);
                        if (expr.offset == null || param < 0 || expr.kind.startsWith("opaque_"))
                            unknown++;
                        write(out, "store", function, entry, op, "ok", expr, param,
                            op.getInput(2), "-", "-", "-", 0, 0, 0, 0);
                        stores++;
                    } else if (code == PcodeOp.CALL || code == PcodeOp.CALLIND) {
                        StringBuilder argIndices = new StringBuilder();
                        for (int i = 1; i < op.getNumInputs(); i++) {
                            AddressExpr expr = decode(op.getInput(i), 0, new HashSet<Varnode>());
                            if (parameterIndex(high, expr.base) == 0) {
                                if (argIndices.length() > 0) argIndices.append(",");
                                argIndices.append(i - 1);
                            }
                        }
                        if (argIndices.length() > 0) escapes++;
                        write(out, "call", function, entry, op, "ok", null, -1,
                            null, code == PcodeOp.CALL ? "CALL" : "CALLIND",
                            token(op.getInput(0)),
                            argIndices.length() == 0 ? "-" : argIndices.toString(),
                            0, 0, 0, 0);
                        calls++;
                    }
                }
                write(out, "summary", function, entry, null, "ok", null, -1,
                    null, "-", "-", "-", stores, calls, unknown, escapes);
                completed++;
            }
        } finally {
            decompiler.dispose();
        }
        println("CONTROLS_FIELD_ALIAS_FORMAT=" + FORMAT);
        println("CONTROLS_FIELD_ALIAS_METHODS=" + entries.size());
        println("CONTROLS_FIELD_ALIAS_DECOMPILED=" + completed);
        println("CONTROLS_FIELD_ALIAS_FAILED=" + failed);
        println("CONTROLS_FIELD_ALIAS_OUTPUT=" + output.getAbsolutePath());
        println("CONTROLS_FIELD_ALIAS_AFFECTS_ROUTING=no");
    }
}
