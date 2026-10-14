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

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;

// Evidence-only use-def export for a direct accessor return value. The exporter
// follows only value-preserving COPY/CAST/INDIRECT/MULTIEQUAL edges. A class
// consumer is recorded only when that value is the first data argument (`this`)
// of a resolved direct CALL. It never infers a class from the accessor name.
public class ExportAccessorReturnConsumers extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraAccessorReturnConsumers/1";
    private static final int MAX_DEPTH = 10;
    private static final int MAX_VALUES = 128;
    private static final int MAX_CALLERS = 256;

    private String cell(Object value) {
        if (value == null) return "-";
        return String.valueOf(value).replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private Function resolveFunction(String token) throws Exception {
        Address address = null;
        try { address = toAddr(token); } catch (Exception ignored) { }
        if (address != null) {
            Function f = currentProgram.getFunctionManager().getFunctionAt(address);
            if (f == null) f = currentProgram.getFunctionManager().getFunctionContaining(address);
            if (f != null) return f;
        }

        Function exact = null;
        Function partial = null;
        int partialCount = 0;
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        while (it.hasNext() && !monitor.isCancelled()) {
            Function f = it.next();
            String shortName = f.getName();
            String fullName = f.getName(true);
            if (token.equals(shortName) || token.equals(fullName)) {
                exact = f;
                break;
            }
            if (shortName.contains(token) || fullName.contains(token)) {
                partial = f;
                partialCount++;
            }
        }
        if (exact != null) return exact;
        if (partialCount == 1) return partial;
        if (partialCount > 1) throw new IllegalArgumentException("Ambiguous function token: " + token);
        throw new IllegalArgumentException("Function not found: " + token);
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

    private boolean isTransparent(PcodeOp op) {
        if (op == null) return false;
        int code = op.getOpcode();
        return code == PcodeOp.COPY || code == PcodeOp.CAST || code == PcodeOp.INDIRECT || code == PcodeOp.MULTIEQUAL;
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

    private void writeHeader(BufferedWriter out) throws Exception {
        out.write("format\trow_type\taccessor_entry\taccessor_name\tdirect_caller_functions\tcaller_functions_decompiled\t" +
            "caller_functions_failed\taccessor_call_sites\taccessor_calls_with_output\tdirect_member_uses\tunsupported_uses\t" +
            "caller_entry\tcaller_name\taccessor_call_site\tuse_depth\tuse_opcode\tuse_address\tuse_input_index\t" +
            "direct_target_entry\tdirect_target_name\tstatus");
        out.newLine();
    }

    private void writeUse(BufferedWriter out, Function accessor, Function caller, Address callSite, int depth,
            PcodeOp use, int inputIndex, Function target, String status) throws Exception {
        out.write(String.join("\t",
            FORMAT, "use", cell(accessor.getEntryPoint()), cell(accessor.getName(true)),
            "-", "-", "-", "-", "-", "-", "-",
            cell(caller.getEntryPoint()), cell(caller.getName(true)), cell(callSite), Integer.toString(depth),
            cell(use == null ? null : use.getMnemonic()),
            cell(use == null ? null : use.getSeqnum().getTarget()), Integer.toString(inputIndex),
            cell(target == null ? null : target.getEntryPoint()), cell(target == null ? null : target.getName(true)), cell(status)));
        out.newLine();
    }

    private int inputIndex(PcodeOp op, Varnode value) {
        if (op == null || value == null) return -1;
        for (int i = 0; i < op.getNumInputs(); i++) if (value.equals(op.getInput(i))) return i;
        return -1;
    }

    private int[] walkUses(BufferedWriter out, Function accessor, Function caller, Address callSite, Varnode root) throws Exception {
        int memberUses = 0;
        int unsupported = 0;
        if (root == null) return new int[]{memberUses, unsupported};

        ArrayDeque<Varnode> values = new ArrayDeque<Varnode>();
        ArrayDeque<Integer> depths = new ArrayDeque<Integer>();
        HashSet<Varnode> seen = new HashSet<Varnode>();
        values.add(root); depths.add(0);
        int visited = 0;

        while (!values.isEmpty() && visited < MAX_VALUES && !monitor.isCancelled()) {
            Varnode value = values.removeFirst();
            int depth = depths.removeFirst();
            if (!seen.add(value)) continue;
            visited++;
            Iterator<PcodeOp> uses = value.getDescendants();
            while (uses.hasNext()) {
                PcodeOp use = uses.next();
                int idx = inputIndex(use, value);
                if (idx < 0) continue;

                if (use.getOpcode() == PcodeOp.CALL) {
                    Function target = resolveCallTarget(use);
                    if (idx == 1 && target != null) {
                        memberUses++;
                        writeUse(out, accessor, caller, callSite, depth, use, idx, target, "direct_this_call");
                    } else {
                        unsupported++;
                        writeUse(out, accessor, caller, callSite, depth, use, idx, target, "direct_call_non_this_or_unresolved");
                    }
                    continue;
                }

                if (depth < MAX_DEPTH && transparentValueInput(use, value) && use.getOutput() != null) {
                    values.addLast(use.getOutput());
                    depths.addLast(depth + 1);
                    continue;
                }

                unsupported++;
                writeUse(out, accessor, caller, callSite, depth, use, idx, null, "nontransparent_use");
            }
        }
        return new int[]{memberUses, unsupported};
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) throw new IllegalArgumentException("Usage: ExportAccessorReturnConsumers.java <function-token> <output.tsv>");
        Function accessor = resolveFunction(args[0]);
        File output = new File(args[1]);
        File parent = output.getParentFile(); if (parent != null) parent.mkdirs();

        Set<Function> callers = directCallers(accessor);
        int decompiled = 0, failed = 0, callSites = 0, callsWithOutput = 0, memberUses = 0, unsupported = 0;
        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException("Could not open current program");
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(output), StandardCharsets.UTF_8))) {
            writeHeader(out);
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
                    Varnode value = op.getOutput();
                    if (value == null) {
                        writeUse(out, accessor, caller, op.getSeqnum().getTarget(), 0, op, -1, target, "accessor_call_without_output");
                        continue;
                    }
                    callsWithOutput++;
                    int[] counts = walkUses(out, accessor, caller, op.getSeqnum().getTarget(), value);
                    memberUses += counts[0];
                    unsupported += counts[1];
                }
            }
            String status;
            if (callers.isEmpty()) status = "no_direct_callers";
            else if (failed != 0) status = "caller_decompile_incomplete";
            else if (callSites == 0) status = "no_accessor_call_sites";
            else status = "complete_return_use_export";
            out.write(String.join("\t",
                FORMAT, "summary", cell(accessor.getEntryPoint()), cell(accessor.getName(true)),
                Integer.toString(callers.size()), Integer.toString(decompiled), Integer.toString(failed),
                Integer.toString(callSites), Integer.toString(callsWithOutput), Integer.toString(memberUses),
                Integer.toString(unsupported), "-", "-", "-", "-", "-", "-", "-", "-", "-", status));
            out.newLine();
        } finally {
            decompiler.dispose();
        }

        println("ACCESSOR_RETURN_CONSUMERS_FORMAT=" + FORMAT);
        println("ACCESSOR_RETURN_CONSUMERS_FUNCTION=" + accessor.getEntryPoint() + " " + accessor.getName(true));
        println("ACCESSOR_RETURN_CONSUMERS_CALLERS=" + callers.size());
        println("ACCESSOR_RETURN_CONSUMERS_CALL_SITES=" + callSites);
        println("ACCESSOR_RETURN_CONSUMERS_MEMBER_USES=" + memberUses);
        println("ACCESSOR_RETURN_CONSUMERS_UNSUPPORTED_USES=" + unsupported);
        println("ACCESSOR_RETURN_CONSUMERS_OUTPUT=" + output.getAbsolutePath());
        println("ACCESSOR_RETURN_CONSUMERS_AFFECTS_ROUTING=no");
    }
}
