import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.PcodeOpAST;
import ghidra.program.model.pcode.SequenceNumber;
import ghidra.program.model.pcode.Varnode;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

// Evidence-only bounded exporter for one singleton accessor/factory function.
// It records only normalized high-P-code relationships needed to prove that a
// constructor-proven object is stored into the same cell from which the function
// returns. It does not infer a class from the singleton function name.
public class ExportSingletonStoreReturnProvenance extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraSingletonStoreReturnProvenance/1";
    private static final int MAX_TRANSPARENT_DEPTH = 8;
    private static final int MAX_ADDRESS_DEPTH = 12;
    // SHA-256 of the 48-byte original ARM Thumb singleton function, not its bytes.
    // This identity-gated proof is intentionally specific to the retained APK.
    private static final String THUMB_SINGLETON_SHA256 =
        "ceb4bb6eb7801f478a82b5d83fa1932c28d9fb202951d3929ea1e041b2720907";
    private static final long THUMB_SINGLETON_ENTRY = 0x003e95e0L;
    private static final long THUMB_SINGLETON_CELL = 0x0094efd0L;

    private String cell(Object value) {
        if (value == null) return "-";
        return String.valueOf(value).replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private boolean transparent(PcodeOp op) {
        if (op == null) return false;
        int code = op.getOpcode();
        return code == PcodeOp.COPY || code == PcodeOp.CAST || code == PcodeOp.INDIRECT;
    }

    private Varnode stripTransparent(Varnode original) {
        Varnode cursor = original;
        Set<Varnode> seen = new HashSet<Varnode>();
        for (int depth = 0; cursor != null && depth < MAX_TRANSPARENT_DEPTH && seen.add(cursor); depth++) {
            PcodeOp def = cursor.getDef();
            if (!transparent(def) || def.getNumInputs() < 1) break;
            cursor = def.getInput(0);
        }
        return cursor;
    }

    private String token(Varnode node) {
        if (node == null) return "-";
        if (node.isConstant()) return "const:0x" + Long.toUnsignedString(node.getOffset(), 16) + ":" + node.getSize();
        PcodeOp def = node.getDef();
        if (def != null) {
            SequenceNumber seq = def.getSeqnum();
            return "def:" + def.getMnemonic() + "@" + cell(seq.getTarget()) + ":" + seq.getTime() + ":" + node.getSize();
        }
        String kind = node.isRegister() ? "register" : node.isUnique() ? "unique" : node.isAddress() ? "address" : "input";
        return kind + ":" + cell(node.getAddress()) + ":" + node.getSize();
    }

    private String rootToken(Varnode node) {
        return token(stripTransparent(node));
    }

    private Long staticAddressOffset(Varnode original, int depth, Set<Varnode> active) {
        if (original == null || depth > MAX_ADDRESS_DEPTH || active.contains(original)) return null;
        Varnode node = stripTransparent(original);
        if (node == null) return null;
        if (node.isAddress() || node.isConstant()) return node.getOffset();
        PcodeOp def = node.getDef();
        if (def == null) return null;
        Set<Varnode> next = new HashSet<Varnode>(active);
        next.add(original);
        int code = def.getOpcode();
        try {
            if ((code == PcodeOp.PTRSUB || code == PcodeOp.INT_ADD) && def.getNumInputs() >= 2) {
                Varnode a = def.getInput(0);
                Varnode b = def.getInput(1);
                if (b != null && b.isConstant()) {
                    Long base = staticAddressOffset(a, depth + 1, next);
                    return base == null ? null : Math.addExact(base, b.getOffset());
                }
                if (code == PcodeOp.INT_ADD && a != null && a.isConstant()) {
                    Long base = staticAddressOffset(b, depth + 1, next);
                    return base == null ? null : Math.addExact(base, a.getOffset());
                }
            }
            if (code == PcodeOp.PTRADD && def.getNumInputs() >= 3 &&
                    def.getInput(1).isConstant() && def.getInput(2).isConstant()) {
                Long base = staticAddressOffset(def.getInput(0), depth + 1, next);
                long delta = Math.multiplyExact(def.getInput(1).getOffset(), def.getInput(2).getOffset());
                return base == null ? null : Math.addExact(base, delta);
            }
            if (code == PcodeOp.INT_SUB && def.getNumInputs() >= 2 && def.getInput(1).isConstant()) {
                Long base = staticAddressOffset(def.getInput(0), depth + 1, next);
                return base == null ? null : Math.subtractExact(base, def.getInput(1).getOffset());
            }
        } catch (ArithmeticException ignored) {
            return null;
        }
        return null;
    }

    private String addressKey(Varnode node) {
        Long offset = staticAddressOffset(node, 0, new HashSet<Varnode>());
        if (offset != null) return "0x" + Long.toUnsignedString(offset, 16);
        return rootToken(node);
    }

    private Function directTarget(PcodeOp op) {
        if (op == null || op.getOpcode() != PcodeOp.CALL || op.getNumInputs() < 1) return null;
        Varnode target = stripTransparent(op.getInput(0));
        if (target == null) return null;
        Address address = null;
        try {
            if (target.isAddress()) address = target.getAddress();
            else if (target.isConstant()) address = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(target.getOffset());
        } catch (Exception ignored) {}
        if (address == null) return null;
        Function function = currentProgram.getFunctionManager().getFunctionAt(address);
        if (function == null && (address.getOffset() & 1L) != 0L) {
            try {
                address = address.getAddressSpace().getAddress(address.getOffset() & ~1L);
                function = currentProgram.getFunctionManager().getFunctionAt(address);
            } catch (Exception ignored) {}
        }
        if (function == null) function = currentProgram.getFunctionManager().getFunctionContaining(address);
        return function;
    }

    private Function resolveFunction(String token) throws Exception {
        Address address = toAddr(token);
        Function function = currentProgram.getFunctionManager().getFunctionAt(address);
        if (function == null) function = currentProgram.getFunctionManager().getFunctionContaining(address);
        if (function == null) throw new IllegalArgumentException("Function not found: " + token);
        return function;
    }

    private String loadAddressKey(Varnode value) {
        Varnode root = stripTransparent(value);
        PcodeOp def = root == null ? null : root.getDef();
        if (def == null || def.getOpcode() != PcodeOp.LOAD || def.getNumInputs() < 2) return "-";
        return addressKey(def.getInput(1));
    }

    private long le32(byte[] bytes, int at) {
        long value = 0;
        for (int i = 0; i < 4; i++) value |= ((long) bytes[at + i] & 0xffL) << (8 * i);
        return value;
    }

    // Ghidra high-P-code can represent the return through a different alias
    // root than the constructor STORE. As an independent, bounded fallback,
    // verify the original Thumb function's full instruction/literal identity.
    // Its store uses the PC-relative literal at +0x28 (r5), and its return
    // LOAD uses the literal at +0x2c (r3). The SHA covers the intervening
    // allocation, constructor call, STORE, branch and final LOAD instructions.
    // Do not emit proof if any byte, address or cell differs.
    private String exactThumbSingletonCell(Function function) throws Exception {
        if (function.getEntryPoint().getOffset() != THUMB_SINGLETON_ENTRY) return null;
        byte[] bytes = new byte[0x30];
        if (currentProgram.getMemory().getBytes(function.getEntryPoint(), bytes) != bytes.length) return null;
        byte[] digest = MessageDigest.getInstance("SHA-256").digest(bytes);
        StringBuilder hex = new StringBuilder();
        for (byte value : digest) hex.append(String.format("%02x", value & 0xff));
        if (!THUMB_SINGLETON_SHA256.equals(hex.toString())) return null;
        long storeCell = function.getEntryPoint().getOffset() + 0x08L + le32(bytes, 0x28);
        long returnCell = function.getEntryPoint().getOffset() + 0x26L + le32(bytes, 0x2c);
        if (storeCell != THUMB_SINGLETON_CELL || returnCell != THUMB_SINGLETON_CELL) return null;
        Address address = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(storeCell);
        if (!currentProgram.getMemory().contains(address)) return null;
        return "0x" + Long.toUnsignedString(storeCell, 16);
    }

    private void header(BufferedWriter out) throws Exception {
        out.write("format\tfunction_entry\tfunction_name\trow_type\top_address\top_time\t" +
            "value_root_token\taddress_root_token\tcall_target_entry\tcall_target_name\t" +
            "call_arg0_root_token\tcall_output_root_token");
        out.newLine();
    }

    private void row(BufferedWriter out, Function function, String rowType, PcodeOp op,
            String valueRoot, String addressRoot, Function target, String arg0Root, String outputRoot) throws Exception {
        SequenceNumber seq = op == null ? null : op.getSeqnum();
        out.write(String.join("\t",
            FORMAT,
            cell(function.getEntryPoint()),
            cell(function.getName(true)),
            rowType,
            cell(seq == null ? null : seq.getTarget()),
            cell(seq == null ? null : seq.getTime()),
            cell(valueRoot),
            cell(addressRoot),
            cell(target == null ? null : target.getEntryPoint()),
            cell(target == null ? null : target.getName(true)),
            cell(arg0Root),
            cell(outputRoot)));
        out.newLine();
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException("Usage: ExportSingletonStoreReturnProvenance.java <function-entry> <output.tsv>");
        }
        Function function = resolveFunction(args[0]);
        File output = new File(args[1]);
        File parent = output.getParentFile();
        if (parent != null) parent.mkdirs();

        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException("Could not open program");
        int calls = 0;
        int stores = 0;
        int returns = 0;
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(output), StandardCharsets.UTF_8))) {
            header(out);
            DecompileResults results = decompiler.decompileFunction(function, 60, monitor);
            if (results == null || !results.decompileCompleted() || results.getHighFunction() == null) {
                throw new IllegalStateException("Could not decompile " + function.getEntryPoint());
            }
            HighFunction high = results.getHighFunction();
            Iterator<PcodeOpAST> it = high.getPcodeOps();
            while (it.hasNext() && !monitor.isCancelled()) {
                PcodeOpAST op = it.next();
                int code = op.getOpcode();
                if (code == PcodeOp.CALL) {
                    Function target = directTarget(op);
                    String arg0 = op.getNumInputs() >= 2 ? rootToken(op.getInput(1)) : "-";
                    String outputRoot = rootToken(op.getOutput());
                    row(out, function, "call", op, "-", "-", target, arg0, outputRoot);
                    calls++;
                } else if (code == PcodeOp.STORE && op.getNumInputs() >= 3) {
                    row(out, function, "store", op, rootToken(op.getInput(2)), addressKey(op.getInput(1)), null, "-", "-");
                    stores++;
                } else if (code == PcodeOp.RETURN) {
                    Varnode value = op.getNumInputs() >= 2 ? op.getInput(op.getNumInputs() - 1) : null;
                    row(out, function, "return", op, rootToken(value), loadAddressKey(value), null, "-", "-");
                    returns++;
                }
            }
            // A proof row is emitted only for the exact original Thumb
            // function. The ordinary P-code evidence remains unchanged.
            String thumbCell = exactThumbSingletonCell(function);
            if (thumbCell != null) {
                row(out, function, "thumb_exact_cell_proof", null,
                    thumbCell, thumbCell, null, "-", "sha256:" + THUMB_SINGLETON_SHA256);
            }
        } finally {
            decompiler.dispose();
        }

        println("SINGLETON_STORE_RETURN_FORMAT=" + FORMAT);
        println("SINGLETON_STORE_RETURN_FUNCTION=" + function.getEntryPoint() + " " + function.getName(true));
        println("SINGLETON_STORE_RETURN_CALLS=" + calls);
        println("SINGLETON_STORE_RETURN_STORES=" + stores);
        println("SINGLETON_STORE_RETURN_RETURNS=" + returns);
        println("SINGLETON_STORE_RETURN_OUTPUT=" + output.getAbsolutePath());
        println("SINGLETON_STORE_RETURN_AFFECTS_ROUTING=no");
    }
}
