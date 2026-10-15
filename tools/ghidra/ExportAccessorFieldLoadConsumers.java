import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.PcodeOpAST;
import ghidra.program.model.pcode.Varnode;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.util.ArrayDeque;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedHashSet;
import java.util.Set;

// Evidence-only continuation of ExportAccessorReturnConsumers. This exporter
// follows an accessor return through transparent operations and constant pointer
// arithmetic to the first dereference LOAD, records the manager-relative field
// offset, and then follows only the loaded pointer value. It never assigns a
// receiver class or changes runtime routing.
public class ExportAccessorFieldLoadConsumers extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraAccessorFieldLoadConsumers/1";
    private static final int MAX_DEPTH = 12;
    private static final int MAX_VALUES = 256;
    private static final int MAX_CALLERS = 256;

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
        return String.valueOf(value).replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private String hex(long value) {
        if (value < 0) return "-0x" + Long.toUnsignedString(-value, 16);
        return "0x" + Long.toUnsignedString(value, 16);
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

    private boolean looksNumeric(String token) {
        if (token == null) return false;
        return token.matches("(?i)(0x)?[0-9a-f]{6,16}");
    }

    private Function resolveFunction(String input) throws Exception {
        FunctionManager fm = currentProgram.getFunctionManager();
        if (looksNumeric(input)) {
            Address address = toAddr(input);
            Function exact = fm.getFunctionAt(address);
            if (exact != null) return exact;
            long off = address.getOffset();
            long alternate = (off & 1L) == 0L ? (off | 1L) : (off & ~1L);
            try {
                Function thumb = fm.getFunctionAt(address.getAddressSpace().getAddress(alternate));
                if (thumb != null) return thumb;
            } catch (Exception ignored) { }
            throw new IllegalArgumentException("No exact function at numeric token: " + input);
        }

        Function exact = null;
        Function partial = null;
        int partialCount = 0;
        FunctionIterator it = fm.getFunctions(true);
        while (it.hasNext() && !monitor.isCancelled()) {
            Function f = it.next();
            String shortName = f.getName();
            String fullName = f.getName(true);
            if (input.equals(shortName) || input.equals(fullName)) {
                exact = f;
                break;
            }
            if (shortName.contains(input) || fullName.contains(input)) {
                partial = f;
                partialCount++;
            }
        }
        if (exact != null) return exact;
        if (partialCount == 1) return partial;
        if (partialCount > 1) throw new IllegalArgumentException("Ambiguous function token: " + input);
        throw new IllegalArgumentException("Function not found: " + input);
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

    private Set<Function> directCallers(Function callee) {
        LinkedHashSet<Function> out = new LinkedHashSet<Function>();
        ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(callee.getEntryPoint());
        FunctionManager fm = currentProgram.getFunctionManager();
        while (refs.hasNext() && out.size() < MAX_CALLERS) {
            Reference ref = refs.next();
            if (ref == null || ref.getFromAddress() == null) continue;
            Function caller = fm.getFunctionContaining(ref.getFromAddress());
            if (caller != null && !caller.equals(callee)) out.add(caller);
        }
        return out;
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
                    value.equals(op.getInput(0)) && op.getInput(1).isConstant() && op.getInput(2).isConstant()) {
                return Math.multiplyExact(op.getInput(1).getOffset(), op.getInput(2).getOffset());
            }
        } catch (ArithmeticException ignored) {
            return null;
        }
        return null;
    }

    private boolean isNullCheck(PcodeOp op, Varnode value) {
        if (op == null || value == null) return false;
        int code = op.getOpcode();
        if (code != PcodeOp.INT_EQUAL && code != PcodeOp.INT_NOTEQUAL) return false;
        if (op.getNumInputs() != 2) return false;
        Varnode a = op.getInput(0), b = op.getInput(1);
        return (value.equals(a) && b.isConstant() && b.getOffset() == 0L) ||
               (value.equals(b) && a.isConstant() && a.getOffset() == 0L);
    }

    private void header(BufferedWriter out) throws Exception {
        out.write("format\trow_type\taccessor_entry\taccessor_name\tdirect_caller_functions\tcaller_functions_decompiled\t" +
            "caller_functions_failed\taccessor_call_sites\tmanager_field_loads\tmanager_nonfield_uses\t" +
            "loaded_value_receiver_calls\tloaded_value_nonreceiver_calls\tloaded_value_retention_stores\t" +
            "loaded_value_return_escapes\tloaded_value_dereference_reads\tloaded_value_dereference_writes\t" +
            "loaded_value_null_checks\tloaded_value_opaque_uses\tcaller_entry\tcaller_name\taccessor_call_site\t" +
            "load_address\tload_offset\tuse_depth\tuse_opcode\tuse_address\tuse_input_index\tdirect_target_entry\t" +
            "direct_target_name\tuse_kind\tstatus");
        out.newLine();
    }

    private void row(BufferedWriter out, String rowType, Function accessor, Function caller, Address callSite,
            Address loadAddress, String loadOffset, int depth, PcodeOp use, int inputIndex,
            Function target, String useKind, String status) throws Exception {
        out.write(String.join("\t",
            FORMAT, rowType, cell(accessor.getEntryPoint()), cell(accessor.getName(true)),
            "-", "-", "-", "-", "-", "-", "-", "-", "-", "-", "-", "-", "-", "-",
            cell(caller == null ? null : caller.getEntryPoint()), cell(caller == null ? null : caller.getName(true)),
            cell(callSite), cell(loadAddress), loadOffset == null ? "-" : loadOffset,
            depth < 0 ? "-" : Integer.toString(depth), cell(use == null ? null : use.getMnemonic()),
            cell(use == null ? null : use.getSeqnum().getTarget()), inputIndex < 0 ? "-" : Integer.toString(inputIndex),
            cell(target == null ? null : target.getEntryPoint()), cell(target == null ? null : target.getName(true)),
            useKind, status));
        out.newLine();
    }

    // [receiver calls, nonreceiver calls, retention stores, returns, reads, writes, null checks, opaque]
    private int[] walkLoadedValue(BufferedWriter out, Function accessor, Function caller, Address callSite,
            Address loadAddress, long loadOffset, Varnode root) throws Exception {
        int[] counts = new int[8];
        if (root == null) return counts;
        ArrayDeque<Work> queue = new ArrayDeque<Work>();
        queue.addLast(new Work(root, 0L, 0));
        HashSet<String> seen = new HashSet<String>();
        int visited = 0;

        while (!queue.isEmpty() && !monitor.isCancelled()) {
            if (visited >= MAX_VALUES) {
                counts[7]++;
                row(out, "loaded_use", accessor, caller, callSite, loadAddress, hex(loadOffset),
                    MAX_DEPTH, null, -1, null, "opaque_use", "value_limit");
                break;
            }
            Work work = queue.removeFirst();
            String key = token(work.value) + "@" + work.depth;
            if (!seen.add(key)) continue;
            visited++;

            Iterator<PcodeOp> uses = work.value.getDescendants();
            while (uses.hasNext()) {
                PcodeOp use = uses.next();
                int idx = inputIndex(use, work.value);
                if (idx < 0) continue;

                if (work.depth < MAX_DEPTH && transparentValueInput(use, work.value) && use.getOutput() != null) {
                    queue.addLast(new Work(use.getOutput(), 0L, work.depth + 1));
                    continue;
                }

                if (use.getOpcode() == PcodeOp.CALL || use.getOpcode() == PcodeOp.CALLIND) {
                    Function target = use.getOpcode() == PcodeOp.CALL ? resolveCallTarget(use) : null;
                    if (idx == 1) {
                        counts[0]++;
                        row(out, "loaded_use", accessor, caller, callSite, loadAddress, hex(loadOffset),
                            work.depth, use, idx, target, "receiver_call", "observed");
                    } else if (idx >= 2) {
                        counts[1]++;
                        row(out, "loaded_use", accessor, caller, callSite, loadAddress, hex(loadOffset),
                            work.depth, use, idx, target, "nonreceiver_call", "observed");
                    } else {
                        counts[7]++;
                        row(out, "loaded_use", accessor, caller, callSite, loadAddress, hex(loadOffset),
                            work.depth, use, idx, target, "opaque_use", "loaded_pointer_used_as_call_target");
                    }
                    continue;
                }

                if (use.getOpcode() == PcodeOp.STORE) {
                    if (idx == 2) {
                        counts[2]++;
                        row(out, "loaded_use", accessor, caller, callSite, loadAddress, hex(loadOffset),
                            work.depth, use, idx, null, "retention_store", "observed");
                    } else if (idx == 1) {
                        counts[5]++;
                        row(out, "loaded_use", accessor, caller, callSite, loadAddress, hex(loadOffset),
                            work.depth, use, idx, null, "dereference_write", "observed");
                    } else {
                        counts[7]++;
                        row(out, "loaded_use", accessor, caller, callSite, loadAddress, hex(loadOffset),
                            work.depth, use, idx, null, "opaque_use", "store_unexpected_input");
                    }
                    continue;
                }

                if (use.getOpcode() == PcodeOp.RETURN) {
                    counts[3]++;
                    row(out, "loaded_use", accessor, caller, callSite, loadAddress, hex(loadOffset),
                        work.depth, use, idx, null, "return_escape", "observed");
                    continue;
                }

                if (use.getOpcode() == PcodeOp.LOAD && idx == 1) {
                    counts[4]++;
                    row(out, "loaded_use", accessor, caller, callSite, loadAddress, hex(loadOffset),
                        work.depth, use, idx, null, "dereference_read", "observed");
                    continue;
                }

                if (isNullCheck(use, work.value)) {
                    counts[6]++;
                    row(out, "loaded_use", accessor, caller, callSite, loadAddress, hex(loadOffset),
                        work.depth, use, idx, null, "null_check", "observed");
                    continue;
                }

                counts[7]++;
                row(out, "loaded_use", accessor, caller, callSite, loadAddress, hex(loadOffset),
                    work.depth, use, idx, null, "opaque_use",
                    work.depth >= MAX_DEPTH ? "depth_limit_or_nontransparent_use" : "nontransparent_use");
            }
        }
        return counts;
    }

    // [field loads, nonfield uses, receiver calls, nonreceiver calls, retention stores, returns, reads, writes, null checks, opaque]
    private int[] walkManagerResult(BufferedWriter out, Function accessor, Function caller, Address callSite,
            Varnode root) throws Exception {
        int[] total = new int[10];
        if (root == null) {
            total[1]++;
            row(out, "manager_use", accessor, caller, callSite, null, null, 0, null, -1, null,
                "manager_nonfield_use", "accessor_call_without_output");
            return total;
        }

        ArrayDeque<Work> queue = new ArrayDeque<Work>();
        queue.addLast(new Work(root, 0L, 0));
        HashSet<String> seen = new HashSet<String>();
        int visited = 0;

        while (!queue.isEmpty() && !monitor.isCancelled()) {
            if (visited >= MAX_VALUES) {
                total[1]++;
                row(out, "manager_use", accessor, caller, callSite, null, null, MAX_DEPTH, null, -1, null,
                    "manager_nonfield_use", "value_limit");
                break;
            }
            Work work = queue.removeFirst();
            String key = token(work.value) + "@" + Long.toString(work.offset);
            if (!seen.add(key)) continue;
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
                        total[1]++;
                        row(out, "manager_use", accessor, caller, callSite, null, null, work.depth,
                            use, idx, null, "manager_nonfield_use", "offset_overflow");
                    }
                    continue;
                }

                if (use.getOpcode() == PcodeOp.LOAD && idx == 1) {
                    total[0]++;
                    Address loadAddress = use.getSeqnum().getTarget();
                    row(out, "field_load", accessor, caller, callSite, loadAddress, hex(work.offset),
                        work.depth, use, idx, null, "field_load", "observed");
                    int[] loaded = walkLoadedValue(out, accessor, caller, callSite, loadAddress, work.offset, use.getOutput());
                    for (int i = 0; i < loaded.length; i++) total[i + 2] += loaded[i];
                    continue;
                }

                total[1]++;
                row(out, "manager_use", accessor, caller, callSite, null, hex(work.offset), work.depth,
                    use, idx, null, "manager_nonfield_use",
                    work.depth >= MAX_DEPTH ? "depth_limit_or_nonfield_use" : "nonfield_use");
            }
        }
        return total;
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException("Usage: ExportAccessorFieldLoadConsumers.java <function-token> <output.tsv>");
        }
        Function accessor = resolveFunction(args[0]);
        File output = new File(args[1]);
        File parent = output.getParentFile();
        if (parent != null) parent.mkdirs();

        Set<Function> callers = directCallers(accessor);
        int decompiled = 0, failed = 0, callSites = 0;
        int[] total = new int[10];
        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException("Could not open current program");
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(output), StandardCharsets.UTF_8))) {
            header(out);
            for (Function caller : callers) {
                DecompileResults results = decompiler.decompileFunction(caller, 60, monitor);
                if (results == null || !results.decompileCompleted() || results.getHighFunction() == null) {
                    failed++;
                    continue;
                }
                decompiled++;
                Iterator<PcodeOpAST> ops = results.getHighFunction().getPcodeOps();
                while (ops.hasNext() && !monitor.isCancelled()) {
                    PcodeOpAST op = ops.next();
                    if (op.getOpcode() != PcodeOp.CALL) continue;
                    Function target = resolveCallTarget(op);
                    if (target == null || !target.getEntryPoint().equals(accessor.getEntryPoint())) continue;
                    callSites++;
                    int[] counts = walkManagerResult(out, accessor, caller, op.getSeqnum().getTarget(), op.getOutput());
                    for (int i = 0; i < total.length; i++) total[i] += counts[i];
                }
            }
            String status;
            if (callers.isEmpty()) status = "no_direct_callers";
            else if (failed != 0) status = "caller_decompile_incomplete";
            else if (callSites == 0) status = "no_accessor_call_sites";
            else status = "complete_field_load_export";

            out.write(String.join("\t",
                FORMAT, "summary", cell(accessor.getEntryPoint()), cell(accessor.getName(true)),
                Integer.toString(callers.size()), Integer.toString(decompiled), Integer.toString(failed),
                Integer.toString(callSites), Integer.toString(total[0]), Integer.toString(total[1]),
                Integer.toString(total[2]), Integer.toString(total[3]), Integer.toString(total[4]),
                Integer.toString(total[5]), Integer.toString(total[6]), Integer.toString(total[7]),
                Integer.toString(total[8]), Integer.toString(total[9]),
                "-", "-", "-", "-", "-", "-", "-", "-", "-", "-", "-", "-", status));
            out.newLine();
        } finally {
            decompiler.dispose();
        }

        println("ACCESSOR_FIELD_LOAD_CONSUMERS_FORMAT=" + FORMAT);
        println("ACCESSOR_FIELD_LOAD_CONSUMERS_FUNCTION=" + accessor.getEntryPoint() + " " + accessor.getName(true));
        println("ACCESSOR_FIELD_LOAD_CONSUMERS_CALLERS=" + callers.size());
        println("ACCESSOR_FIELD_LOAD_CONSUMERS_CALL_SITES=" + callSites);
        println("ACCESSOR_FIELD_LOAD_CONSUMERS_FIELD_LOADS=" + total[0]);
        println("ACCESSOR_FIELD_LOAD_CONSUMERS_MANAGER_NONFIELD_USES=" + total[1]);
        println("ACCESSOR_FIELD_LOAD_CONSUMERS_RECEIVER_CALLS=" + total[2]);
        println("ACCESSOR_FIELD_LOAD_CONSUMERS_RETENTION_STORES=" + total[4]);
        println("ACCESSOR_FIELD_LOAD_CONSUMERS_OUTPUT=" + output.getAbsolutePath());
        println("ACCESSOR_FIELD_LOAD_CONSUMERS_AFFECTS_ROUTING=no");
    }
}
