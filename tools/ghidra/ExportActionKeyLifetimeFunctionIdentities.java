import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.util.LinkedHashSet;
import java.util.Set;

// Metadata-only helper for EnemyObject action dictionary key-lifetime evidence.
// It reads only predecessor/successor callee entries from the bounded lifetime
// plan and emits Ghidra function identities/signatures for those exact entries.
public class ExportActionKeyLifetimeFunctionIdentities extends GhidraScript {
    private static final String PLAN_FORMAT = "NEVERGONE.EnemyObjectActionKeyLifetimePlan/1";
    private static final String FORMAT = "NEVERGONE.GhidraFunctionIdentity/1";

    private String cell(Object value) {
        if (value == null) return "-";
        return String.valueOf(value).replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private int column(String[] header, String name) {
        for (int i = 0; i < header.length; i++) {
            if (name.equals(header[i])) return i;
        }
        return -1;
    }

    private Set<String> readEntries(File input) throws Exception {
        LinkedHashSet<String> entries = new LinkedHashSet<String>();
        try (BufferedReader in = new BufferedReader(new InputStreamReader(
                new FileInputStream(input), StandardCharsets.UTF_8))) {
            String first = in.readLine();
            if (first == null) throw new IllegalArgumentException("Empty lifetime plan: " + input);
            String[] header = first.split("\\t", -1);
            int format = column(header, "format");
            int predecessor = column(header, "predecessor_entry");
            int successor = column(header, "successor_entry");
            if (format < 0 || predecessor < 0 || successor < 0) {
                throw new IllegalArgumentException(
                    "Missing format/predecessor_entry/successor_entry columns: " + input);
            }
            String line;
            while ((line = in.readLine()) != null) {
                if (line.trim().isEmpty()) continue;
                String[] cols = line.split("\\t", -1);
                int needed = Math.max(format, Math.max(predecessor, successor));
                if (cols.length <= needed) continue;
                if (!PLAN_FORMAT.equals(cols[format])) {
                    throw new IllegalArgumentException("Unexpected lifetime plan format: " + cols[format]);
                }
                String pred = cols[predecessor].trim();
                String succ = cols[successor].trim();
                if (!pred.isEmpty() && !"-".equals(pred)) entries.add(pred);
                if (!succ.isEmpty() && !"-".equals(succ)) entries.add(succ);
            }
        }
        if (entries.isEmpty()) throw new IllegalArgumentException("No helper entries in lifetime plan: " + input);
        return entries;
    }

    private Function resolveExact(String token) throws Exception {
        Address address = toAddr(token);
        FunctionManager functions = currentProgram.getFunctionManager();
        Function function = functions.getFunctionAt(address);
        if (function == null) {
            throw new IllegalArgumentException("Function not found at exact helper entry: " + token);
        }
        return function;
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException(
                "Usage: ExportActionKeyLifetimeFunctionIdentities.java <lifetime-plan.tsv> <output.tsv>");
        }
        File input = new File(args[0]);
        File output = new File(args[1]);
        File parent = output.getParentFile();
        if (parent != null) parent.mkdirs();

        Set<String> entries = readEntries(input);
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(
                new FileOutputStream(output), StandardCharsets.UTF_8))) {
            out.write("format\trequested_entry\tresolved_entry\tshort_name\tqualified_name\tsignature\tis_external");
            out.newLine();
            for (String requested : entries) {
                if (monitor.isCancelled()) break;
                Function function = resolveExact(requested);
                out.write(String.join("\t",
                    FORMAT,
                    cell(requested),
                    cell(function.getEntryPoint()),
                    cell(function.getName()),
                    cell(function.getName(true)),
                    cell(function.getSignature()),
                    Boolean.toString(function.isExternal())));
                out.newLine();
            }
        }
        println("Exported " + entries.size() + " action-key lifetime helper identities");
    }
}
