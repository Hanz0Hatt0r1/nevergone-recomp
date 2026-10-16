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

// Metadata-only helper for caller-argument evidence. It reads caller entries from
// an ExportCallerArgumentSubtree TSV and emits the exact Ghidra function identity
// for those entries. No bytes, decompiler text, or asset data are exported.
public class ExportCallerFunctionIdentitiesLegacy extends GhidraScript {
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

    private Set<String> readCallerEntries(File input) throws Exception {
        LinkedHashSet<String> entries = new LinkedHashSet<String>();
        try (BufferedReader in = new BufferedReader(new InputStreamReader(
                new FileInputStream(input), StandardCharsets.UTF_8))) {
            String first = in.readLine();
            if (first == null) throw new IllegalArgumentException("Empty subtree TSV: " + input);
            String[] header = first.split("\\t", -1);
            int rowType = column(header, "row_type");
            int callerEntry = column(header, "caller_entry");
            if (rowType < 0 || callerEntry < 0) {
                throw new IllegalArgumentException("Missing row_type/caller_entry columns: " + input);
            }
            String line;
            while ((line = in.readLine()) != null) {
                if (line.trim().isEmpty()) continue;
                String[] cols = line.split("\\t", -1);
                if (cols.length <= callerEntry || cols.length <= rowType) continue;
                if (!"node".equals(cols[rowType])) continue;
                String entry = cols[callerEntry].trim();
                if (!entry.isEmpty() && !"-".equals(entry)) entries.add(entry);
            }
        }
        if (entries.isEmpty()) throw new IllegalArgumentException("No caller entries in subtree TSV: " + input);
        return entries;
    }

    private Function resolveExact(String token) throws Exception {
        Address address = toAddr(token);
        FunctionManager functions = currentProgram.getFunctionManager();
        Function function = functions.getFunctionAt(address);
        if (function == null) function = functions.getFunctionContaining(address);
        if (function == null) throw new IllegalArgumentException("Function not found for caller entry: " + token);
        return function;
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException(
                "Usage: ExportCallerFunctionIdentities.java <caller-subtrees.tsv> <output.tsv>");
        }
        File input = new File(args[0]);
        File output = new File(args[1]);
        File parent = output.getParentFile();
        if (parent != null) parent.mkdirs();

        Set<String> entries = readCallerEntries(input);
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
        println("Exported " + entries.size() + " caller function identities from " + input.getName());
    }
}
