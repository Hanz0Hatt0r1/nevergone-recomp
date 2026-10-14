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
import ghidra.program.model.pcode.Varnode;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

// Read-only bounded operand roots for the 15 exact sites emitted by
// triage_nevergone_controls_field_alias_evidence.py. Neither a root with
// parameter index zero nor a matching offset proves a unique receiver class.
public class ExportControlsAliasOperandRoots extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraControlsAliasOperandRoots/1";
    private static final int MAX_DEPTH = 20;

    private static class Site {
        String kind, entry, address, time, target, offset;
        Site(String[] cells) {
            kind = cells[0]; entry = cells[1]; address = cells[2];
            time = cells[3]; target = cells[4]; offset = cells[5];
        }
    }

    private static class Expr {
        final Varnode root;
        final Long offset;
        final String kind;
        Expr(Varnode root, Long offset, String kind) {
            this.root = root; this.offset = offset; this.kind = kind;
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
        if (def != null)
            return "def:" + def.getMnemonic() + "@" +
                cell(def.getSeqnum().getTarget()) + ":" + def.getSeqnum().getTime() +
                ":" + node.getSize();
        String kind = node.isRegister() ? "register" : node.isUnique() ? "unique" :
            node.isAddress() ? "address" : "input";
        return kind + ":" + cell(node.getAddress()) + ":" + node.getSize();
    }

    private Expr trace(Varnode node, int depth, Set<Varnode> seen) {
        if (node == null || depth > MAX_DEPTH || seen.contains(node))
            return new Expr(null, null, "missing_cycle_or_depth");
        if (node.isConstant()) return new Expr(node, 0L, "constant");
        PcodeOp def = node.getDef();
        if (def == null) return new Expr(node, 0L, "leaf");
        Set<Varnode> next = new HashSet<Varnode>(seen);
        next.add(node);
        int code = def.getOpcode();
        if ((code == PcodeOp.COPY || code == PcodeOp.CAST ||
             code == PcodeOp.INDIRECT) && def.getNumInputs() >= 1)
            return trace(def.getInput(0), depth + 1, next);
        if (code == PcodeOp.LOAD) return new Expr(node, 0L, "load");
        if (code == PcodeOp.MULTIEQUAL) return new Expr(node, null, "phi_ambiguous");
        if (def.getNumInputs() >= 2 &&
            (code == PcodeOp.PTRSUB || code == PcodeOp.PTRADD ||
             code == PcodeOp.INT_ADD || code == PcodeOp.INT_SUB)) {
            Varnode a = def.getInput(0);
            Varnode b = def.getInput(1);
            Long delta = null;
            Varnode base = a;
            try {
                if (b != null && b.isConstant()) {
                    delta = b.getOffset();
                    if (code == PcodeOp.INT_SUB) delta = Math.negateExact(delta);
                    if (code == PcodeOp.PTRADD) {
                        if (def.getNumInputs() < 3 || !def.getInput(2).isConstant())
                            return new Expr(node, null, "ptradd_unknown_scale");
                        delta = Math.multiplyExact(delta, def.getInput(2).getOffset());
                    }
                } else if (code == PcodeOp.INT_ADD && a != null && a.isConstant()) {
                    delta = a.getOffset();
                    base = b;
                }
                if (delta != null) {
                    Expr child = trace(base, depth + 1, next);
                    if (child.offset == null) return child;
                    return new Expr(child.root, Math.addExact(child.offset, delta),
                        "base_plus_constant");
                }
            } catch (ArithmeticException ignored) {
                return new Expr(node, null, "offset_overflow");
            }
        }
        return new Expr(node, null, "opaque_" + def.getMnemonic());
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
        return value < 0 ? "-0x" + Long.toUnsignedString(-value, 16) :
            "0x" + Long.toUnsignedString(value, 16);
    }

    private String index(int value) { return value < 0 ? "-" : Integer.toString(value); }

    private String[] exprCells(HighFunction high, Expr expr) {
        return new String[] { cell(expr.kind), token(expr.root), hex(expr.offset),
            index(parameterIndex(high, expr.root)) };
    }

    private String row(String[] cells) { return String.join("\t", cells); }

    private void writeHeader(BufferedWriter out) throws Exception {
        out.write("format\trow_type\tkind\tfunction_entry\tsite_address\tsite_time\t" +
            "site_status\toperand_index\toperand_role\toperand_token\troot_kind\t" +
            "root_token\troot_offset\troot_parameter_index\tload_address_root_kind\t" +
            "load_address_root_token\tload_address_offset\tload_address_parameter_index\t" +
            "nested_load_address_root_kind\tnested_load_address_root_token\t" +
            "nested_load_address_offset\tnested_load_address_parameter_index\toperand_count");
        out.newLine();
    }

    private void writeRow(BufferedWriter out, Site site, String type, String status,
            int operand, String role, Varnode node, HighFunction high, int count) throws Exception {
        Expr root = node == null ? new Expr(null, null, "missing") :
            trace(node, 0, new HashSet<Varnode>());
        Expr loadAddr = new Expr(null, null, "not_load");
        Expr nested = new Expr(null, null, "not_nested_load");
        if (root.root != null) {
            PcodeOp def = root.root.getDef();
            if (def != null && def.getOpcode() == PcodeOp.LOAD && def.getNumInputs() >= 2) {
                loadAddr = trace(def.getInput(1), 0, new HashSet<Varnode>());
                PcodeOp childDef = loadAddr.root == null ? null : loadAddr.root.getDef();
                if (childDef != null && childDef.getOpcode() == PcodeOp.LOAD &&
                    childDef.getNumInputs() >= 2)
                    nested = trace(childDef.getInput(1), 0, new HashSet<Varnode>());
            }
        }
        List<String> cells = new ArrayList<String>();
        cells.add(FORMAT); cells.add(type); cells.add(site.kind);
        cells.add(site.entry); cells.add(site.address); cells.add(site.time);
        cells.add(status); cells.add(index(operand)); cells.add(role);
        cells.add(token(node));
        for (String item : exprCells(high, root)) cells.add(item);
        for (String item : exprCells(high, loadAddr)) cells.add(item);
        for (String item : exprCells(high, nested)) cells.add(item);
        cells.add(count < 0 ? "-" : Integer.toString(count));
        out.write(row(cells.toArray(new String[0])));
        out.newLine();
    }

    private List<Site> readPlan(String path) throws Exception {
        List<String> lines = Files.readAllLines(Paths.get(path), StandardCharsets.UTF_8);
        if (lines.isEmpty() || !lines.get(0).equals(
            "kind\tfunction_entry\top_address\top_time\ttarget_or_base\toffset"))
            throw new IllegalArgumentException("unexpected alias triage plan header");
        List<Site> sites = new ArrayList<Site>();
        Set<String> unique = new HashSet<String>();
        int stores = 0, indirect = 0, factories = 0;
        for (int i = 1; i < lines.size(); i++) {
            if (lines.get(i).trim().isEmpty()) continue;
            String[] values = lines.get(i).split("\t", -1);
            if (values.length != 6 || !values[1].matches("[0-9a-f]{8}") ||
                !values[2].matches("[0-9a-f]{8}") || !values[3].matches("[0-9]+"))
                throw new IllegalArgumentException("invalid plan row " + i);
            Site site = new Site(values);
            if (!unique.add(site.entry + ":" + site.address + ":" + site.time))
                throw new IllegalArgumentException("duplicate plan site");
            if ("store_loaded_pointer_base".equals(site.kind)) stores++;
            else if ("indirect_unresolved".equals(site.kind)) indirect++;
            else if ("controlsbutton_factory_callback_candidate".equals(site.kind)) factories++;
            else throw new IllegalArgumentException("unexpected plan kind: " + site.kind);
            sites.add(site);
        }
        if (sites.size() != 15 || stores != 9 || indirect != 3 || factories != 3)
            throw new IllegalArgumentException("expected 9 STORE, 3 CALLIND, 3 factory sites");
        return sites;
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) throw new IllegalArgumentException(
            "Usage: ExportControlsAliasOperandRoots.java <plan.tsv> <output.tsv>");
        List<Site> sites = readPlan(args[0]);
        File output = new File(args[1]);
        if (output.getParentFile() != null) output.getParentFile().mkdirs();

        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram))
            throw new IllegalStateException("cannot open Ghidra program");
        Map<String, HighFunction> cache = new HashMap<String, HighFunction>();
        int ok = 0;
        int missing = 0;
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(
                new FileOutputStream(output), StandardCharsets.UTF_8))) {
            writeHeader(out);
            for (Site site : sites) {
                if (monitor.isCancelled()) throw new IllegalStateException("probe cancelled");
                HighFunction high = cache.get(site.entry);
                if (high == null) {
                    Address addr = toAddr(site.entry);
                    Function function = currentProgram.getFunctionManager().getFunctionAt(addr);
                    if (function == null)
                        throw new IllegalStateException("missing function " + site.entry);
                    DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
                    if (result == null || !result.decompileCompleted() ||
                        result.getHighFunction() == null)
                        throw new IllegalStateException("decompile failed " + site.entry);
                    high = result.getHighFunction();
                    cache.put(site.entry, high);
                }
                PcodeOpAST found = null;
                Iterator<PcodeOpAST> it = high.getPcodeOps();
                while (it.hasNext()) {
                    PcodeOpAST op = it.next();
                    if (site.address.equals(cell(op.getSeqnum().getTarget())) &&
                        site.time.equals(Long.toString(op.getSeqnum().getTime()))) {
                        if (found != null) throw new IllegalStateException("duplicate P-code site");
                        found = op;
                    }
                }
                int expected = "store_loaded_pointer_base".equals(site.kind) ?
                    PcodeOp.STORE : "indirect_unresolved".equals(site.kind) ?
                    PcodeOp.CALLIND : PcodeOp.CALL;
                if (found == null || found.getOpcode() != expected) {
                    writeRow(out, site, "summary", found == null ? "missing" :
                        "opcode_mismatch", -1, "-", null, high, 0);
                    missing++;
                    continue;
                }
                List<Integer> operands = new ArrayList<Integer>();
                if (expected == PcodeOp.STORE) {
                    if (found.getNumInputs() >= 3) {
                        operands.add(1); operands.add(2);
                    }
                } else {
                    for (int i = 0; i < found.getNumInputs() && i < 32; i++)
                        operands.add(i);
                }
                if (operands.isEmpty() || found.getNumInputs() > 32) {
                    writeRow(out, site, "summary", "operand_shape_mismatch",
                        -1, "-", null, high, 0);
                    missing++;
                    continue;
                }
                writeRow(out, site, "summary", "ok", -1, "-", null, high,
                    operands.size());
                for (int operand : operands) {
                    String role = expected == PcodeOp.STORE ?
                        (operand == 1 ? "store_destination" : "store_value") :
                        (operand == 0 ? "call_target" : "call_arg_" + (operand - 1));
                    writeRow(out, site, "operand", "ok", operand, role,
                        found.getInput(operand), high, -1);
                }
                ok++;
            }
        } finally {
            decompiler.dispose();
        }
        println("CONTROLS_ALIAS_OPERAND_ROOT_FORMAT=" + FORMAT);
        println("CONTROLS_ALIAS_OPERAND_ROOT_SITES=" + sites.size());
        println("CONTROLS_ALIAS_OPERAND_ROOT_COMPLETE=" + ok);
        println("CONTROLS_ALIAS_OPERAND_ROOT_MISSING=" + missing);
        println("CONTROLS_ALIAS_OPERAND_ROOT_OUTPUT=" + output.getAbsolutePath());
        println("CONTROLS_ALIAS_OPERAND_ROOT_AFFECTS_ROUTING=no");
    }
}
