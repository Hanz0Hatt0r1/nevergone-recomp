import ghidra.app.script.GhidraScript;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

// Nevergone adapter: turns caller-entry rows into a generic identity plan and
// restores the historical format. The original script is the offline fallback.
public class ExportCallerFunctionIdentities extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraFunctionIdentity/1";
    private static final String GENERIC_HEADER = "format\tplan_id\trequested_entry\tresolved_entry\tshort_name\tqualified_name\tsignature\tis_external";
    private static final String LEGACY_HEADER = "format\trequested_entry\tresolved_entry\tshort_name\tqualified_name\tsignature\tis_external";

    private int column(String[] header, String name) {
        for (int i = 0; i < header.length; i++) if (name.equals(header[i])) return i;
        return -1;
    }

    private String cell(String value) {
        return value.replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private List<String> readEntries(File input) throws Exception {
        Set<String> entries = new LinkedHashSet<String>();
        try (BufferedReader in = new BufferedReader(new InputStreamReader(new FileInputStream(input), StandardCharsets.UTF_8))) {
            String first = in.readLine();
            if (first == null) throw new IllegalArgumentException("Empty subtree TSV: " + input);
            String[] header = first.split("\\t", -1);
            int rowType = column(header, "row_type");
            int callerEntry = column(header, "caller_entry");
            if (rowType < 0 || callerEntry < 0) throw new IllegalArgumentException("Missing row_type/caller_entry columns: " + input);
            String line;
            while ((line = in.readLine()) != null) {
                if (line.trim().isEmpty()) continue;
                String[] values = line.split("\\t", -1);
                if (values.length <= Math.max(rowType, callerEntry) || !"node".equals(values[rowType])) continue;
                String entry = values[callerEntry].trim();
                if (!entry.isEmpty() && !"-".equals(entry)) entries.add(entry);
            }
        }
        if (entries.isEmpty()) throw new IllegalArgumentException("No caller entries in subtree TSV: " + input);
        return new ArrayList<String>(entries);
    }

    private void writePlan(File output, List<String> entries) throws Exception {
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(output), StandardCharsets.UTF_8))) {
            out.write("plan_id\trequested_entry\tresolution");
            out.newLine();
            for (int i = 0; i < entries.size(); i++) {
                out.write(i + "\t" + cell(entries.get(i)) + "\tcontaining");
                out.newLine();
            }
        }
    }

    private void writeLegacy(File generic, File output, List<String> entries) throws Exception {
        try (BufferedReader in = new BufferedReader(new InputStreamReader(new FileInputStream(generic), StandardCharsets.UTF_8));
             BufferedWriter out = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(output), StandardCharsets.UTF_8))) {
            if (!GENERIC_HEADER.equals(in.readLine())) throw new IllegalArgumentException("Unexpected generic function identity header");
            out.write(LEGACY_HEADER);
            out.newLine();
            String line;
            int index = 0;
            while ((line = in.readLine()) != null) {
                if (line.trim().isEmpty()) continue;
                String[] values = line.split("\\t", -1);
                if (values.length != 8 || index >= entries.size() || !Integer.toString(index).equals(values[1]) || !entries.get(index).equals(values[2]))
                    throw new IllegalArgumentException("Generic identity output does not match caller plan");
                out.write(String.join("\t", FORMAT, values[2], values[3], values[4], values[5], values[6], values[7]));
                out.newLine();
                index++;
            }
            if (index != entries.size()) throw new IllegalArgumentException("Incomplete generic function identity output");
        }
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2 && args.length != 3)
            throw new IllegalArgumentException("Usage: ExportCallerFunctionIdentities.java <caller-subtrees.tsv> <output.tsv> [shared-ghidra-script-dir]");
        File output = new File(args[1]);
        File parent = output.getAbsoluteFile().getParentFile();
        if (parent != null) parent.mkdirs();
        File shared = args.length == 3 ? new File(args[2], "ExportGhidraFunctionIdentities.java") : null;
        if (shared == null || !shared.isFile()) {
            runScript("ExportCallerFunctionIdentitiesLegacy.java", new String[] {args[0], args[1]});
            return;
        }
        List<String> entries = readEntries(new File(args[0]));
        File plan = File.createTempFile("caller-identities-plan-", ".tsv", parent);
        File generic = File.createTempFile("caller-identities-output-", ".tsv", parent);
        try {
            writePlan(plan, entries);
            runScript("ExportGhidraFunctionIdentities.java", new String[] {plan.getAbsolutePath(), generic.getAbsolutePath()});
            writeLegacy(generic, output, entries);
        } finally {
            plan.delete();
            generic.delete();
        }
        println("Exported " + entries.size() + " caller function identities from " + new File(args[0]).getName());
    }
}
