import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
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
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;

// Evidence-only exporter for the recovered EnemyObject max/current-health
// stores. For one target function it follows the stored value backwards only
// through value-preserving copies/casts/conversions and reports a source LOAD
// when that LOAD address resolves to parameter + constant offset.
//
// No field is named as GameLevelsEnemyData health by this script. The output is
// structural metadata only and intentionally contains no instruction bytes,
// disassembly text, decompiler C text, strings, or proprietary data.
public class ExportEnemyHealthFieldConsumer extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.EnemyHealthFieldConsumer/1";
    private static final long MAX_HEALTH_OFFSET = 0x160L;
    private static final long CURRENT_HEALTH_OFFSET = 0x164L;
    private static final int MAX_ADDRESS_DEPTH = 24;
    private static final int MAX_VALUE_DEPTH = 24;

    private static final class Expr {
        final Varnode base;
        final Long offset;
        final String kind;

        Expr(Varnode base, Long offset, String kind) {
            this.base = base;
            this.offset = offset;
            this.kind = kind;
        }
    }

    private static final class SourceLoad {
        final int parameterIndex;
        final long offset;
        final int size;
        final String address;
        final int time;

        SourceLoad(int parameterIndex, long offset, int size, String address, int time) {
            this.parameterIndex = parameterIndex;
            this.offset = offset;
            this.size = size;
            this.address = address;
            this.time = time;
        }

        String key() {
            return parameterIndex + ":" + Long.toUnsignedString(offset, 16) + ":" + size;
        }
    }

    private String cell(Object value) {
        if (value == null) return "-";
        return String.valueOf(value).replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private String hex(long value) {
        return value < 0
            ? "-0x" + Long.toUnsignedString(-value, 16)
            : "0x" + Long.toUnsignedString(value, 16);
    }

    private Function resolveFunction(String token) throws Exception {
        Address address = null;
        try {
            address = toAddr(token);
        } catch (Exception ignored) {
            // Fall through to name lookup.
        }
        if (address != null) {
            Function function = currentProgram.getFunctionManager().getFunctionAt(address);
            if (function == null) {
                function = currentProgram.getFunctionManager().getFunctionContaining(address);
            }
            if (function != null) return function;
        }

        Function exact = null;
        Function partial = null;
        int partialCount = 0;
        FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
        while (functions.hasNext() && !monitor.isCancelled()) {
            Function function = functions.next();
            String shortName = function.getName();
            String fullName = function.getName(true);
            if (token.equals(shortName) || token.equals(fullName)) {
                exact = function;
                break;
            }
            if (shortName.contains(token) || fullName.contains(token)) {
                partial = function;
                partialCount++;
            }
        }
        if (exact != null) return exact;
        if (partialCount == 1) return partial;
        if (partialCount > 1) {
            throw new IllegalArgumentException("Ambiguous function token: " + token);
        }
        throw new IllegalArgumentException("Function not found: " + token);
    }

    private boolean addressTransparent(PcodeOp op) {
        if (op == null) return false;
        int code = op.getOpcode();
        return code == PcodeOp.COPY || code == PcodeOp.CAST || code == PcodeOp.INDIRECT;
    }

    private Expr decodeAddress(Varnode node, int depth, Set<Varnode> active) {
        if (node == null || depth > MAX_ADDRESS_DEPTH || active.contains(node)) {
            return new Expr(null, null, "missing_or_cycle");
        }
        if (node.isConstant()) return new Expr(null, node.getOffset(), "constant");

        PcodeOp def = node.getDef();
        if (def == null) return new Expr(node, 0L, "base");
        Set<Varnode> next = new HashSet<Varnode>(active);
        next.add(node);

        if (addressTransparent(def) && def.getNumInputs() >= 1) {
            return decodeAddress(def.getInput(0), depth + 1, next);
        }

        int code = def.getOpcode();
        if (code == PcodeOp.MULTIEQUAL) {
            return new Expr(null, null, "phi_ambiguous");
        }

        try {
            if (def.getNumInputs() >= 2 &&
                    (code == PcodeOp.PTRSUB || code == PcodeOp.INT_ADD ||
                     code == PcodeOp.INT_SUB || code == PcodeOp.PTRADD)) {
                Varnode a = def.getInput(0);
                Varnode b = def.getInput(1);
                Varnode base = a;
                Long delta = null;

                if (b != null && b.isConstant()) {
                    delta = b.getOffset();
                    if (code == PcodeOp.INT_SUB) delta = Math.negateExact(delta);
                    if (code == PcodeOp.PTRADD) {
                        if (def.getNumInputs() < 3 || !def.getInput(2).isConstant()) {
                            return new Expr(null, null, "ptradd_unknown_scale");
                        }
                        delta = Math.multiplyExact(delta, def.getInput(2).getOffset());
                    }
                } else if (code == PcodeOp.INT_ADD && a != null && a.isConstant()) {
                    delta = a.getOffset();
                    base = b;
                }

                if (delta != null) {
                    Expr child = decodeAddress(base, depth + 1, next);
                    if (child.offset == null) {
                        return new Expr(null, null, "unresolved_arithmetic_base");
                    }
                    return new Expr(
                        child.base,
                        Math.addExact(child.offset, delta),
                        "base_plus_constant");
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
        for (int index = 0; index < map.getNumParams(); ++index) {
            HighSymbol symbol = map.getParamSymbol(index);
            if (symbol == null || symbol.getHighVariable() == null) continue;
            HighVariable parameter = symbol.getHighVariable();
            if (candidate == parameter) return index;
            if (candidate.getRepresentative() != null && parameter.getRepresentative() != null &&
                    candidate.getRepresentative().equals(parameter.getRepresentative())) {
                return index;
            }
        }
        return -1;
    }

    private boolean valueTransparent(PcodeOp op) {
        if (op == null) return false;
        int code = op.getOpcode();
        return code == PcodeOp.COPY ||
               code == PcodeOp.CAST ||
               code == PcodeOp.INDIRECT ||
               code == PcodeOp.INT_ZEXT ||
               code == PcodeOp.INT_SEXT ||
               code == PcodeOp.FLOAT_INT2FLOAT ||
               code == PcodeOp.FLOAT_FLOAT2FLOAT;
    }

    private void collectSourceLoads(
            HighFunction high,
            Varnode node,
            int depth,
            Set<Varnode> active,
            Map<String, SourceLoad> sources) {
        if (node == null || depth > MAX_VALUE_DEPTH || active.contains(node)) return;
        PcodeOp def = node.getDef();
        if (def == null) return;

        Set<Varnode> next = new HashSet<Varnode>(active);
        next.add(node);

        if (def.getOpcode() == PcodeOp.LOAD && def.getNumInputs() >= 2) {
            Expr address = decodeAddress(def.getInput(1), 0, new HashSet<Varnode>());
            int parameter = parameterIndex(high, address.base);
            if (parameter < 0 || address.offset == null) return;
            SequenceNumber sequence = def.getSeqnum();
            SourceLoad source = new SourceLoad(
                parameter,
                address.offset,
                node.getSize(),
                cell(sequence == null ? null : sequence.getTarget()),
                sequence == null ? -1 : sequence.getTime());
            sources.put(source.key(), source);
            return;
        }

        if (valueTransparent(def) && def.getNumInputs() >= 1) {
            collectSourceLoads(high, def.getInput(0), depth + 1, next, sources);
            return;
        }

        if (def.getOpcode() == PcodeOp.SUBPIECE && def.getNumInputs() >= 1) {
            collectSourceLoads(high, def.getInput(0), depth + 1, next, sources);
            return;
        }

        if (def.getOpcode() == PcodeOp.MULTIEQUAL) {
            for (int index = 0; index < def.getNumInputs(); ++index) {
                collectSourceLoads(high, def.getInput(index), depth + 1, next, sources);
            }
        }
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException(
                "Usage: ExportEnemyHealthFieldConsumer.java <function-address-or-name> <output.tsv>");
        }

        Function function = resolveFunction(args[0]);
        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) {
            throw new IllegalStateException("Could not open current program in decompiler");
        }

        DecompileResults results = decompiler.decompileFunction(function, 60, monitor);
        if (results == null || !results.decompileCompleted() || results.getHighFunction() == null) {
            String message = results == null ? "no result" : results.getErrorMessage();
            decompiler.dispose();
            throw new IllegalStateException(
                "Could not decompile " + function.getName(true) + ": " + message);
        }
        HighFunction high = results.getHighFunction();

        File output = new File(args[1]);
        if (output.getParentFile() != null) output.getParentFile().mkdirs();
        int targetStores = 0;

        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(
                new FileOutputStream(output), StandardCharsets.UTF_8))) {
            out.write(
                "format\tfunction_entry\tfunction_name\tstore_address\tstore_time\t" +
                "store_base_parameter_index\tstore_offset\tvalue_size\tsource_count\t" +
                "source_parameter_index\tsource_offset\tsource_size\t" +
                "source_load_address\tsource_load_time\ttrace_status");
            out.newLine();

            Iterator<PcodeOpAST> operations = high.getPcodeOps();
            while (operations.hasNext() && !monitor.isCancelled()) {
                PcodeOpAST op = operations.next();
                if (op.getOpcode() != PcodeOp.STORE || op.getNumInputs() < 3) continue;

                Expr destination = decodeAddress(op.getInput(1), 0, new HashSet<Varnode>());
                int baseParameter = parameterIndex(high, destination.base);
                if (baseParameter != 0 || destination.offset == null) continue;
                if (destination.offset != MAX_HEALTH_OFFSET &&
                        destination.offset != CURRENT_HEALTH_OFFSET) {
                    continue;
                }

                Varnode value = op.getInput(2);
                Map<String, SourceLoad> sources = new LinkedHashMap<String, SourceLoad>();
                collectSourceLoads(
                    high, value, 0, new HashSet<Varnode>(), sources);

                SourceLoad unique = sources.size() == 1
                    ? sources.values().iterator().next()
                    : null;
                String status = sources.isEmpty()
                    ? "unresolved"
                    : sources.size() == 1 ? "unique_parameter_load" : "ambiguous_parameter_loads";
                SequenceNumber sequence = op.getSeqnum();

                out.write(String.join("\t",
                    FORMAT,
                    cell(function.getEntryPoint()),
                    cell(function.getName(true)),
                    cell(sequence == null ? null : sequence.getTarget()),
                    sequence == null ? "-" : Integer.toString(sequence.getTime()),
                    Integer.toString(baseParameter),
                    hex(destination.offset),
                    value == null ? "-" : Integer.toString(value.getSize()),
                    Integer.toString(sources.size()),
                    unique == null ? "-" : Integer.toString(unique.parameterIndex),
                    unique == null ? "-" : hex(unique.offset),
                    unique == null ? "-" : Integer.toString(unique.size),
                    unique == null ? "-" : unique.address,
                    unique == null || unique.time < 0 ? "-" : Integer.toString(unique.time),
                    status));
                out.newLine();
                targetStores++;
            }
        } finally {
            decompiler.dispose();
        }

        println("ENEMY_HEALTH_FIELD_CONSUMER_FORMAT=" + FORMAT);
        println("ENEMY_HEALTH_FIELD_CONSUMER_FUNCTION=" +
            function.getEntryPoint() + ":" + function.getName(true));
        println("ENEMY_HEALTH_FIELD_CONSUMER_TARGET_STORES=" + targetStores);
        println("ENEMY_HEALTH_FIELD_CONSUMER_OUTPUT=" + output.getAbsolutePath());
        println("ENEMY_HEALTH_FIELD_CONSUMER_SEMANTIC_CLAIM=none");
    }
}
