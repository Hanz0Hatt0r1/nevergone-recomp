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
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

// Read-only bounded probe for the 78 call arguments that the earlier Controls
// inventory classified as derived from high-P-code parameter zero. This probe
// adds the missing constant byte offset relative to that parameter. A recovered
// offset does not prove that a callee mutates the object and never changes routing.
public class ExportControlsThisCallArgumentOffsets extends GhidraScript {
    private static final String PLAN_FORMAT = "NEVERGONE.ControlsThisCallArgumentPlan/1";
    private static final String FORMAT = "NEVERGONE.GhidraControlsThisCallArgumentOffsets/1";
    private static final int MAX_DEPTH = 20;

    private static class Site {
        final String functionEntry;
        final String opAddress;
        final long opTime;
        final String callKind;
        final String targetToken;
        final String calleeCategory;
        final String calleeSymbol;
        final int argumentIndex;

        Site(String[] cells) {
            functionEntry = cells[1];
            opAddress = cells[2];
            opTime = Long.parseLong(cells[3]);
            callKind = cells[4];
            targetToken = cells[5];
            calleeCategory = cells[6];
            calleeSymbol = cells[7];
            argumentIndex = Integer.parseInt(cells[8]);
        }
    }

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
        if (node.isConstant()) {
            return "const:0x" + Long.toUnsignedString(node.getOffset(), 16) + ":" + node.getSize();
        }
        PcodeOp definition = node.getDef();
        if (definition != null) {
            SequenceNumber sequence = definition.getSeqnum();
            return "def:" + definition.getMnemonic() + "@" + cell(sequence.getTarget()) + ":" +
                sequence.getTime() + ":" + node.getSize();
        }
        String kind = node.isRegister() ? "register" : node.isUnique() ? "unique" :
            node.isAddress() ? "address" : "input";
        return kind + ":" + cell(node.getAddress()) + ":" + node.getSize();
    }

    private boolean transparent(PcodeOp operation) {
        if (operation == null) return false;
        int code = operation.getOpcode();
        return code == PcodeOp.COPY || code == PcodeOp.CAST || code == PcodeOp.INDIRECT;
    }

    // Keep this decoder intentionally aligned with ExportControlsFieldWriteAliasInventory:
    // the old exporter used the same normalized expression to decide which arguments
    // were parameter-zero-derived. This probe records the offset that was previously
    // discarded instead of changing the classification semantics.
    private AddressExpr decode(Varnode node, int depth, Set<Varnode> active) {
        if (node == null || depth > MAX_DEPTH || active.contains(node)) {
            return new AddressExpr(null, null, "missing_or_cycle");
        }
        if (node.isConstant()) return new AddressExpr(null, node.getOffset(), "constant");
        PcodeOp definition = node.getDef();
        if (definition == null) return new AddressExpr(node, 0L, "base");

        Set<Varnode> next = new HashSet<Varnode>(active);
        next.add(node);
        if (transparent(definition) && definition.getNumInputs() >= 1) {
            return decode(definition.getInput(0), depth + 1, next);
        }

        int code = definition.getOpcode();
        if (code == PcodeOp.MULTIEQUAL) {
            return new AddressExpr(null, null, "phi_ambiguous");
        }
        try {
            if (definition.getNumInputs() >= 2 &&
                    (code == PcodeOp.PTRSUB || code == PcodeOp.INT_ADD ||
                     code == PcodeOp.INT_SUB || code == PcodeOp.PTRADD)) {
                Varnode left = definition.getInput(0);
                Varnode right = definition.getInput(1);
                Long delta = null;
                Varnode base = left;
                if (right != null && right.isConstant()) {
                    delta = right.getOffset();
                    if (code == PcodeOp.INT_SUB) delta = Math.negateExact(delta);
                    if (code == PcodeOp.PTRADD) {
                        if (definition.getNumInputs() < 3 || !definition.getInput(2).isConstant()) {
                            return new AddressExpr(null, null, "ptradd_unknown_scale");
                        }
                        delta = Math.multiplyExact(delta, definition.getInput(2).getOffset());
                    }
                } else if (code == PcodeOp.INT_ADD && left != null && left.isConstant()) {
                    delta = left.getOffset();
                    base = right;
                }
                if (delta != null) {
                    AddressExpr child = decode(base, depth + 1, next);
                    if (child.offset == null) {
                        return new AddressExpr(null, null, "unresolved_arithmetic_base");
                    }
                    return new AddressExpr(
                        child.base,
                        Math.addExact(child.offset, delta),
                        "base_plus_constant");
                }
            }
        } catch (ArithmeticException ignored) {
            return new AddressExpr(null, null, "offset_overflow");
        }
        return new AddressExpr(node, 0L, "opaque_" + definition.getMnemonic());
    }

    private int parameterIndex(HighFunction high, Varnode node) {
        if (high == null || node == null || node.getHigh() == null) return -1;
        LocalSymbolMap map = high.getLocalSymbolMap();
        if (map == null) return -1;
        HighVariable candidate = node.getHigh();
        for (int i = 0; i < map.getNumParams(); i++) {
            HighSymbol symbol = map.getParamSymbol(i);
            if (symbol == null || symbol.getHighVariable() == null) continue;
            HighVariable parameter = symbol.getHighVariable();
            if (candidate == parameter) return i;
            if (candidate.getRepresentative() != null && parameter.getRepresentative() != null &&
                    candidate.getRepresentative().equals(parameter.getRepresentative())) return i;
        }
        return -1;
    }

    private String hex(Long value) {
        if (value == null) return "-";
        if (value.longValue() < 0) {
            return "-0x" + Long.toUnsignedString(-value.longValue(), 16);
        }
        return "0x" + Long.toUnsignedString(value.longValue(), 16);
    }

    private List<Site> readPlan(String path) throws Exception {
        List<Site> sites = new ArrayList<Site>();
        Set<String> unique = new HashSet<String>();
        try (BufferedReader input = Files.newBufferedReader(Paths.get(path), StandardCharsets.UTF_8)) {
            String header = input.readLine();
            if (!"format\tfunction_entry\top_address\top_time\tcall_kind\tcall_target_token\tcallee_category\tcallee_symbol\targument_index".equals(header)) {
                throw new IllegalArgumentException("unexpected Controls call-argument plan header");
            }
            String line;
            while ((line = input.readLine()) != null) {
                if (line.trim().isEmpty()) continue;
                String[] cells = line.split("\t", -1);
                if (cells.length != 9 || !PLAN_FORMAT.equals(cells[0]) ||
                        !cells[1].matches("[0-9a-f]{8}") || !cells[2].matches("[0-9a-f]{8}") ||
                        !cells[3].matches("[0-9]+") || !("CALL".equals(cells[4]) || "CALLIND".equals(cells[4])) ||
                        !cells[8].matches("[0-9]+")) {
                    throw new IllegalArgumentException("invalid plan row: " + line);
                }
                Site site = new Site(cells);
                if (site.argumentIndex < 0 || site.argumentIndex > 30) {
                    throw new IllegalArgumentException("argument index outside bounded range");
                }
                String key = site.functionEntry + ":" + site.opAddress + ":" + site.opTime;
                if (!unique.add(key)) throw new IllegalArgumentException("duplicate planned call site: " + key);
                sites.add(site);
            }
        }
        if (sites.size() != 78) throw new IllegalArgumentException("expected exactly 78 planned call sites");
        return sites;
    }

    private HighFunction decompile(DecompInterface decompiler, String entry) throws Exception {
        Address address = toAddr(entry);
        Function function = currentProgram.getFunctionManager().getFunctionAt(address);
        if (function == null) throw new IllegalStateException("missing function: " + entry);
        DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
        if (result == null || !result.decompileCompleted() || result.getHighFunction() == null) {
            throw new IllegalStateException("decompile failed: " + entry);
        }
        return result.getHighFunction();
    }

    private void header(BufferedWriter out) throws Exception {
        out.write("format\trow_type\tfunction_entry\top_address\top_time\tcall_kind\t" +
            "planned_target_token\tobserved_target_token\tcallee_category\tcallee_symbol\t" +
            "argument_index\toperand_index\toperand_token\taddress_kind\taddress_root_token\t" +
            "address_offset\troot_parameter_index\tstatus\tselected_argument_count");
        out.newLine();
    }

    private void summary(BufferedWriter out, Site site, String observedTarget, String status, int count) throws Exception {
        out.write(String.join("\t",
            FORMAT, "summary", site.functionEntry, site.opAddress, Long.toString(site.opTime),
            site.callKind, site.targetToken, cell(observedTarget), site.calleeCategory,
            site.calleeSymbol, Integer.toString(site.argumentIndex), "-", "-", "-", "-", "-", "-",
            status, Integer.toString(count)));
        out.newLine();
    }

    private void argument(BufferedWriter out, Site site, String observedTarget, Varnode operand,
            AddressExpr decoded, int parameterIndex) throws Exception {
        out.write(String.join("\t",
            FORMAT, "argument", site.functionEntry, site.opAddress, Long.toString(site.opTime),
            site.callKind, site.targetToken, cell(observedTarget), site.calleeCategory,
            site.calleeSymbol, Integer.toString(site.argumentIndex),
            Integer.toString(site.argumentIndex + 1), token(operand), decoded.kind,
            token(decoded.base), hex(decoded.offset), parameterIndex < 0 ? "-" : Integer.toString(parameterIndex),
            "ok", "-"));
        out.newLine();
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException(
                "Usage: ExportControlsThisCallArgumentOffsets.java <plan.tsv> <output.tsv>");
        }
        List<Site> sites = readPlan(args[0]);
        File output = new File(args[1]);
        if (output.getParentFile() != null) output.getParentFile().mkdirs();

        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) {
            throw new IllegalStateException("could not open Ghidra program");
        }
        Map<String, HighFunction> cache = new HashMap<String, HighFunction>();
        int complete = 0;
        int incomplete = 0;
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(
                new FileOutputStream(output), StandardCharsets.UTF_8))) {
            header(out);
            for (Site site : sites) {
                if (monitor.isCancelled()) throw new IllegalStateException("probe cancelled");
                HighFunction high = cache.get(site.functionEntry);
                if (high == null) {
                    high = decompile(decompiler, site.functionEntry);
                    cache.put(site.functionEntry, high);
                }

                PcodeOpAST found = null;
                Iterator<PcodeOpAST> operations = high.getPcodeOps();
                while (operations.hasNext()) {
                    PcodeOpAST operation = operations.next();
                    if (site.opAddress.equals(cell(operation.getSeqnum().getTarget())) &&
                            site.opTime == operation.getSeqnum().getTime()) {
                        if (found != null) throw new IllegalStateException("duplicate P-code operation at planned site");
                        found = operation;
                    }
                }
                int expectedOpcode = "CALLIND".equals(site.callKind) ? PcodeOp.CALLIND : PcodeOp.CALL;
                if (found == null || found.getOpcode() != expectedOpcode) {
                    summary(out, site, "-", found == null ? "missing_site" : "opcode_mismatch", 0);
                    incomplete++;
                    continue;
                }
                String observedTarget = token(found.getInput(0));
                if (!site.targetToken.equals(observedTarget)) {
                    summary(out, site, observedTarget, "target_token_mismatch", 0);
                    incomplete++;
                    continue;
                }
                int operandIndex = site.argumentIndex + 1;
                if (operandIndex >= found.getNumInputs()) {
                    summary(out, site, observedTarget, "argument_missing", 0);
                    incomplete++;
                    continue;
                }
                Varnode operand = found.getInput(operandIndex);
                AddressExpr decoded = decode(operand, 0, new HashSet<Varnode>());
                int param = parameterIndex(high, decoded.base);
                summary(out, site, observedTarget, "ok", 1);
                argument(out, site, observedTarget, operand, decoded, param);
                complete++;
            }
        } finally {
            decompiler.dispose();
        }

        println("CONTROLS_THIS_CALL_ARGUMENT_FORMAT=" + FORMAT);
        println("CONTROLS_THIS_CALL_ARGUMENT_SITES=" + sites.size());
        println("CONTROLS_THIS_CALL_ARGUMENT_COMPLETE=" + complete);
        println("CONTROLS_THIS_CALL_ARGUMENT_INCOMPLETE=" + incomplete);
        println("CONTROLS_THIS_CALL_ARGUMENT_OUTPUT=" + output.getAbsolutePath());
        println("CONTROLS_THIS_CALL_ARGUMENT_AFFECTS_ROUTING=no");
    }
}
