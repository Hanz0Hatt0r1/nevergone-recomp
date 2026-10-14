import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.HighSymbol;
import ghidra.program.model.pcode.HighVariable;
import ghidra.program.model.pcode.LocalSymbolMap;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.Varnode;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

// Evidence-only bounded exporter for uses of external callee parameters that
// receive the unadjusted Controls* pointer. It does not infer receiver classes.
public class ExportControlsFullThisCalleeParameterUses extends GhidraScript {
    private static final String PLAN_FORMAT = "NEVERGONE.ControlsFullThisCalleeFrontierPlan/1";
    private static final String FORMAT = "NEVERGONE.GhidraControlsFullThisCalleeParameterUses/1";
    private static final int MAX_DEPTH = 20;
    private static final int MAX_VALUES = 512;

    private static class Site {
        final String sourceFunctionEntry;
        final String sourceOpAddress;
        final String sourceOpTime;
        final String sourceCallKind;
        final String targetToken;
        final String category;
        final String symbol;
        final int argumentIndex;
        final String siteKind;
        final String calleeEntry;
        final int calleeParameterIndex;

        Site(String[] c) {
            sourceFunctionEntry = c[1];
            sourceOpAddress = c[2];
            sourceOpTime = c[3];
            sourceCallKind = c[4];
            targetToken = c[5];
            category = c[6];
            symbol = c[7];
            argumentIndex = Integer.parseInt(c[8]);
            siteKind = c[9];
            calleeEntry = c[10];
            calleeParameterIndex = "-".equals(c[11]) ? -1 : Integer.parseInt(c[11]);
        }
    }

    private static class Work {
        final Varnode value;
        final long offset;
        final int depth;
        Work(Varnode value, long offset, int depth) {
            this.value = value;
            this.offset = offset;
            this.depth = depth;
        }
    }

    private String cell(Object value) {
        if (value == null) return "-";
        return String.valueOf(value).replace('\t', ' ').replace('\r', ' ').replace('\n', ' ');
    }

    private String token(Varnode node) {
        if (node == null) return "-";
        if (node.isConstant()) return "const:0x" + Long.toUnsignedString(node.getOffset(), 16) + ":" + node.getSize();
        PcodeOp def = node.getDef();
        if (def != null) {
            return "def:" + def.getMnemonic() + "@" + cell(def.getSeqnum().getTarget()) + ":" +
                def.getSeqnum().getTime() + ":" + node.getSize();
        }
        String kind = node.isRegister() ? "register" : node.isUnique() ? "unique" :
            node.isAddress() ? "address" : "input";
        return kind + ":" + cell(node.getAddress()) + ":" + node.getSize();
    }

    private String hex(long value) {
        if (value < 0) return "-0x" + Long.toUnsignedString(-value, 16);
        return "0x" + Long.toUnsignedString(value, 16);
    }

    private int inputIndex(PcodeOp op, Varnode value) {
        if (op == null || value == null) return -1;
        for (int i = 0; i < op.getNumInputs(); i++) {
            if (value.equals(op.getInput(i))) return i;
        }
        return -1;
    }

    private boolean isTransparent(PcodeOp op) {
        if (op == null) return false;
        int code = op.getOpcode();
        return code == PcodeOp.COPY || code == PcodeOp.CAST ||
               code == PcodeOp.INDIRECT || code == PcodeOp.MULTIEQUAL;
    }

    private boolean transparentValueInput(PcodeOp op, Varnode value) {
        if (!isTransparent(op) || value == null) return false;
        if (op.getOpcode() == PcodeOp.INDIRECT) {
            return op.getNumInputs() >= 1 && value.equals(op.getInput(0));
        }
        for (int i = 0; i < op.getNumInputs(); i++) {
            if (value.equals(op.getInput(i))) return true;
        }
        return false;
    }

    private Long arithmeticDelta(PcodeOp op, Varnode value) {
        if (op == null || value == null) return null;
        int code = op.getOpcode();
        try {
            if (code == PcodeOp.PTRSUB && op.getNumInputs() >= 2 &&
                    value.equals(op.getInput(0)) && op.getInput(1).isConstant()) {
                return op.getInput(1).getOffset();
            }
            if (code == PcodeOp.INT_ADD && op.getNumInputs() >= 2) {
                if (value.equals(op.getInput(0)) && op.getInput(1).isConstant()) {
                    return op.getInput(1).getOffset();
                }
                if (value.equals(op.getInput(1)) && op.getInput(0).isConstant()) {
                    return op.getInput(0).getOffset();
                }
            }
            if (code == PcodeOp.INT_SUB && op.getNumInputs() >= 2 &&
                    value.equals(op.getInput(0)) && op.getInput(1).isConstant()) {
                return Math.negateExact(op.getInput(1).getOffset());
            }
            if (code == PcodeOp.PTRADD && op.getNumInputs() >= 3 &&
                    value.equals(op.getInput(0)) && op.getInput(1).isConstant() &&
                    op.getInput(2).isConstant()) {
                return Math.multiplyExact(op.getInput(1).getOffset(), op.getInput(2).getOffset());
            }
        } catch (ArithmeticException ignored) {
            return null;
        }
        return null;
    }

    private Address callTargetAddress(PcodeOp op) {
        if (op == null || op.getOpcode() != PcodeOp.CALL || op.getNumInputs() < 1) return null;
        Varnode target = op.getInput(0);
        if (target == null) return null;
        if (!target.isConstant()) return target.getAddress();
        try {
            return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(target.getOffset());
        } catch (Exception ignored) {
            return null;
        }
    }

    private Function resolveCallTarget(PcodeOp op) {
        Address address = callTargetAddress(op);
        if (address == null) return null;
        FunctionManager fm = currentProgram.getFunctionManager();
        Function f = fm.getFunctionAt(address);
        if (f != null) return f;
        long off = address.getOffset();
        if ((off & 1L) == 0L) return null;
        try {
            return fm.getFunctionAt(address.getAddressSpace().getAddress(off & ~1L));
        } catch (Exception ignored) {
            return null;
        }
    }

    private List<Site> readPlan(String path) throws Exception {
        List<Site> out = new ArrayList<Site>();
        Set<String> unique = new HashSet<String>();
        try (BufferedReader input = Files.newBufferedReader(Paths.get(path), StandardCharsets.UTF_8)) {
            String expected = "format\tsource_function_entry\tsource_op_address\tsource_op_time\tsource_call_kind\t" +
                "call_target_token\tcallee_category\tcallee_symbol\targument_index\tsite_kind\tcallee_entry\tcallee_parameter_index";
            String header = input.readLine();
            if (!expected.equals(header)) throw new IllegalArgumentException("unexpected full-this frontier plan header");
            String line;
            while ((line = input.readLine()) != null) {
                if (line.trim().isEmpty()) continue;
                String[] c = line.split("\t", -1);
                if (c.length != 12 || !PLAN_FORMAT.equals(c[0]) ||
                        !c[1].matches("[0-9a-f]{8}") || !c[2].matches("[0-9a-f]{8}") ||
                        !c[3].matches("[0-9]+") || !c[8].matches("[0-9]+")) {
                    throw new IllegalArgumentException("invalid plan row: " + line);
                }
                Site site = new Site(c);
                String key = site.sourceFunctionEntry + ":" + site.sourceOpAddress + ":" + site.sourceOpTime;
                if (!unique.add(key)) throw new IllegalArgumentException("duplicate plan site: " + key);
                out.add(site);
            }
        }
        if (out.size() != 14) throw new IllegalArgumentException("expected exactly 14 external full-this sites");
        return out;
    }

    private HighFunction decompile(DecompInterface decompiler, Function function) {
        DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
        if (result == null || !result.decompileCompleted() || result.getHighFunction() == null) return null;
        return result.getHighFunction();
    }

    private void header(BufferedWriter out) throws Exception {
        out.write("format\trow_type\tsource_function_entry\tsource_op_address\tcallee_entry\tcallee_name\t" +
            "callee_parameter_index\tdecompile_status\tparameter_instances\tdirect_write_count\tretention_store_count\t" +
            "forwarded_call_count\treturn_escape_count\tdereference_read_count\topaque_use_count\tuse_depth\t" +
            "use_opcode\tuse_address\tuse_input_index\tuse_kind\tvalue_offset\ttarget_entry\ttarget_name\t" +
            "target_argument_index\tdestination_token\tvalue_token\tstatus");
        out.newLine();
    }

    private void writeUse(BufferedWriter out, Site site, Function callee, int parameterInstances,
            Work work, PcodeOp use, int inputIndex, String useKind, Function target,
            int targetArgumentIndex, String destinationToken, String status) throws Exception {
        out.write(String.join("\t",
            FORMAT, "use", site.sourceFunctionEntry, site.sourceOpAddress,
            cell(callee == null ? site.calleeEntry : callee.getEntryPoint()),
            cell(callee == null ? site.symbol : callee.getName(true)),
            Integer.toString(site.calleeParameterIndex), "-", Integer.toString(parameterInstances),
            "-", "-", "-", "-", "-", "-",
            Integer.toString(work.depth), cell(use == null ? null : use.getMnemonic()),
            cell(use == null ? null : use.getSeqnum().getTarget()), Integer.toString(inputIndex),
            useKind, hex(work.offset),
            cell(target == null ? null : target.getEntryPoint()),
            cell(target == null ? null : target.getName(true)),
            targetArgumentIndex < 0 ? "-" : Integer.toString(targetArgumentIndex),
            destinationToken == null ? "-" : destinationToken,
            token(work.value), status));
        out.newLine();
    }

    private int[] walkUses(BufferedWriter out, Site site, Function callee, HighVariable parameter) throws Exception {
        int directWrites = 0, retentionStores = 0, forwards = 0, returns = 0, reads = 0, opaque = 0;
        Varnode[] instances = parameter == null ? null : parameter.getInstances();
        int instanceCount = instances == null ? 0 : instances.length;
        if (instances == null || instances.length == 0) return new int[]{0,0,0,0,0,0,0,0};

        ArrayDeque<Work> queue = new ArrayDeque<Work>();
        for (Varnode value : instances) if (value != null) queue.addLast(new Work(value, 0L, 0));
        Set<String> seen = new HashSet<String>();
        int visited = 0;
        boolean truncated = false;

        while (!queue.isEmpty() && !monitor.isCancelled()) {
            if (visited >= MAX_VALUES) { truncated = true; break; }
            Work work = queue.removeFirst();
            String seenKey = token(work.value) + "@" + Long.toString(work.offset);
            if (!seen.add(seenKey)) continue;
            visited++;

            Iterator<PcodeOp> uses = work.value.getDescendants();
            while (uses.hasNext()) {
                PcodeOp use = uses.next();
                int idx = inputIndex(use, work.value);
                if (idx < 0) continue;

                if (work.depth < MAX_DEPTH && transparentValueInput(use, work.value) && use.getOutput() != null) {
                    queue.addLast(new Work(use.getOutput(), work.offset, work.depth + 1));
                    continue;
                }

                Long delta = arithmeticDelta(use, work.value);
                if (work.depth < MAX_DEPTH && delta != null && use.getOutput() != null) {
                    try {
                        queue.addLast(new Work(use.getOutput(), Math.addExact(work.offset, delta.longValue()), work.depth + 1));
                    } catch (ArithmeticException ignored) {
                        opaque++;
                        writeUse(out, site, callee, instanceCount, work, use, idx,
                            "opaque_use", null, -1, null, "offset_overflow");
                    }
                    continue;
                }

                if (use.getOpcode() == PcodeOp.STORE) {
                    if (idx == 1) {
                        directWrites++;
                        writeUse(out, site, callee, instanceCount, work, use, idx,
                            "direct_field_write", null, -1,
                            use.getNumInputs() >= 3 ? token(use.getInput(1)) : null, "observed");
                    } else if (idx == 2) {
                        retentionStores++;
                        writeUse(out, site, callee, instanceCount, work, use, idx,
                            "retention_store", null, -1,
                            use.getNumInputs() >= 2 ? token(use.getInput(1)) : null, "observed");
                    } else {
                        opaque++;
                        writeUse(out, site, callee, instanceCount, work, use, idx,
                            "opaque_use", null, -1, null, "store_unexpected_input");
                    }
                    continue;
                }

                if (use.getOpcode() == PcodeOp.CALL || use.getOpcode() == PcodeOp.CALLIND) {
                    if (idx >= 1) {
                        forwards++;
                        Function target = use.getOpcode() == PcodeOp.CALL ? resolveCallTarget(use) : null;
                        writeUse(out, site, callee, instanceCount, work, use, idx,
                            use.getOpcode() == PcodeOp.CALL ? "direct_forward_call" : "indirect_forward_call",
                            target, idx - 1, null, target == null && use.getOpcode() == PcodeOp.CALL ?
                                "direct_target_unresolved" : "observed");
                    } else {
                        opaque++;
                        writeUse(out, site, callee, instanceCount, work, use, idx,
                            "opaque_use", null, -1, null, "parameter_used_as_call_target");
                    }
                    continue;
                }

                if (use.getOpcode() == PcodeOp.RETURN) {
                    returns++;
                    writeUse(out, site, callee, instanceCount, work, use, idx,
                        "return_escape", null, -1, null, "observed");
                    continue;
                }

                if (use.getOpcode() == PcodeOp.LOAD && idx == 1) {
                    reads++;
                    writeUse(out, site, callee, instanceCount, work, use, idx,
                        "dereference_read", null, -1, null, "observed");
                    continue;
                }

                opaque++;
                writeUse(out, site, callee, instanceCount, work, use, idx,
                    "opaque_use", null, -1, null,
                    work.depth >= MAX_DEPTH ? "depth_limit_or_nontransparent_use" : "nontransparent_use");
            }
        }
        return new int[]{directWrites, retentionStores, forwards, returns, reads, opaque, instanceCount, truncated ? 1 : 0};
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) throw new IllegalArgumentException(
            "Usage: ExportControlsFullThisCalleeParameterUses.java <plan.tsv> <output.tsv>");
        List<Site> sites = readPlan(args[0]);
        File output = new File(args[1]);
        File parent = output.getParentFile(); if (parent != null) parent.mkdirs();

        int direct = 0, indirect = 0, complete = 0, incomplete = 0;
        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException("Could not open current program");

        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(
                new FileOutputStream(output), StandardCharsets.UTF_8))) {
            header(out);
            for (Site site : sites) {
                if (!"direct_external".equals(site.siteKind)) {
                    indirect++;
                    continue;
                }
                direct++;
                Function callee = currentProgram.getFunctionManager().getFunctionAt(toAddr(site.calleeEntry));
                if (callee == null) {
                    incomplete++;
                    out.write(String.join("\t",
                        FORMAT, "summary", site.sourceFunctionEntry, site.sourceOpAddress,
                        site.calleeEntry, site.symbol, Integer.toString(site.calleeParameterIndex),
                        "missing_function", "0", "0", "0", "0", "0", "0", "0",
                        "-", "-", "-", "-", "-", "-", "-", "-", "-", "-", "-",
                        "callee_missing"));
                    out.newLine();
                    continue;
                }

                HighFunction high = decompile(decompiler, callee);
                if (high == null) {
                    incomplete++;
                    out.write(String.join("\t",
                        FORMAT, "summary", site.sourceFunctionEntry, site.sourceOpAddress,
                        cell(callee.getEntryPoint()), cell(callee.getName(true)),
                        Integer.toString(site.calleeParameterIndex), "failed", "0",
                        "0", "0", "0", "0", "0", "0",
                        "-", "-", "-", "-", "-", "-", "-", "-", "-", "-", "-",
                        "decompile_failed"));
                    out.newLine();
                    continue;
                }

                LocalSymbolMap map = high.getLocalSymbolMap();
                HighSymbol symbol = map == null || site.calleeParameterIndex < 0 ||
                    site.calleeParameterIndex >= map.getNumParams() ? null : map.getParamSymbol(site.calleeParameterIndex);
                HighVariable parameter = symbol == null ? null : symbol.getHighVariable();
                if (parameter == null) {
                    incomplete++;
                    out.write(String.join("\t",
                        FORMAT, "summary", site.sourceFunctionEntry, site.sourceOpAddress,
                        cell(callee.getEntryPoint()), cell(callee.getName(true)),
                        Integer.toString(site.calleeParameterIndex), "ok", "0",
                        "0", "0", "0", "0", "0", "0",
                        "-", "-", "-", "-", "-", "-", "-", "-", "-", "-", "-",
                        "parameter_missing"));
                    out.newLine();
                    continue;
                }

                int[] counts = walkUses(out, site, callee, parameter);
                String status = counts[7] == 0 ? "complete_parameter_use_export" : "parameter_use_truncated";
                if (counts[7] == 0) complete++; else incomplete++;
                out.write(String.join("\t",
                    FORMAT, "summary", site.sourceFunctionEntry, site.sourceOpAddress,
                    cell(callee.getEntryPoint()), cell(callee.getName(true)),
                    Integer.toString(site.calleeParameterIndex), "ok", Integer.toString(counts[6]),
                    Integer.toString(counts[0]), Integer.toString(counts[1]), Integer.toString(counts[2]),
                    Integer.toString(counts[3]), Integer.toString(counts[4]), Integer.toString(counts[5]),
                    "-", "-", "-", "-", "-", "-", "-", "-", "-", "-", "-", status));
                out.newLine();
            }
        } finally {
            decompiler.dispose();
        }

        println("CONTROLS_FULL_THIS_CALLEE_USE_FORMAT=" + FORMAT);
        println("CONTROLS_FULL_THIS_CALLEE_USE_DIRECT=" + direct);
        println("CONTROLS_FULL_THIS_CALLEE_USE_INDIRECT=" + indirect);
        println("CONTROLS_FULL_THIS_CALLEE_USE_COMPLETE=" + complete);
        println("CONTROLS_FULL_THIS_CALLEE_USE_INCOMPLETE=" + incomplete);
        println("CONTROLS_FULL_THIS_CALLEE_USE_OUTPUT=" + output.getAbsolutePath());
        println("CONTROLS_FULL_THIS_CALLEE_USE_AFFECTS_ROUTING=no");
    }
}
