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

// Nevergone adapter: extracts helper entries from its domain plan and restores
// its historical identity TSV. The original script is the offline fallback.
public class ExportActionKeyLifetimeFunctionIdentities extends GhidraScript {
    private static final String PLAN_FORMAT = "NEVERGONE.EnemyObjectActionKeyLifetimePlan/1";
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
            if (first == null) throw new IllegalArgumentException("Empty lifetime plan: " + input);
            String[] header = first.split("\\t", -1);
            int format = column(header, "format");
            int predecessor = column(header, "predecessor_entry");
            int successor = column(header, "successor_entry");
            if (format < 0 || predecessor < 0 || successor < 0)
                throw new IllegalArgumentException("Missing format/predecessor_entry/successor_entry columns: " + input);
            String line;
            while ((line = in.readLine()) != null) {
                if (line.trim().isEmpty()) continue;
                String[] values = line.split("\\t", -1);
                int needed = Math.max(format, Math.max(predecessor, successor));
                if (values.length <= needed) continue;
                if (!PLAN_FORMAT.equals(values[format])) throw new IllegalArgumentException("Unexpected lifetime plan format: " + values[format]);
                String pred = values[predecessor].trim();
                String succ = values[successor].trim();
                if (!pred.isEmpty() && !"-".equals(pred)) entries.add(pred);
                if (!succ.isEmpty() && !"-".equals(succ)) entries.add(succ);
            }
        }
        if (entries.isEmpty()) throw new IllegalArgumentException("No helper entries in lifetime plan: " + input);
        return new ArrayList<String>(entries);
    }

    private void writePlan(File output, List<String> entries) throws Exception {
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(output), StandardCharsets.UTF_8))) {
            out.write("plan_id\trequested_entry\tresolution");
            out.newLine();
            for (int i = 0; i < entries.size(); i++) {
                out.write(i + "\t" + cell(entries.get(i)) + "\texact");
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
                    throw new IllegalArgumentException("Generic identity output does not match lifetime plan");
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
            throw new IllegalArgumentException("Usage: ExportActionKeyLifetimeFunctionIdentities.java <lifetime-plan.tsv> <output.tsv> [shared-ghidra-script-dir]");
        File output = new File(args[1]);
        File parent = output.getAbsoluteFile().getParentFile();
        if (parent != null) parent.mkdirs();
        File shared = args.length == 3 ? new File(args[2], "ExportGhidraFunctionIdentities.java") : null;
        if (shared == null || !shared.isFile()) {
            runScript("ExportActionKeyLifetimeFunctionIdentitiesLegacy.java", new String[] {args[0], args[1]});
            return;
        }
        List<String> entries = readEntries(new File(args[0]));
        File plan = File.createTempFile("action-key-identities-plan-", ".tsv", parent);
        File generic = File.createTempFile("action-key-identities-output-", ".tsv", parent);
        try {
            writePlan(plan, entries);
            runScript("ExportGhidraFunctionIdentities.java", new String[] {plan.getAbsolutePath(), generic.getAbsolutePath()});
            writeLegacy(generic, output, entries);
        } finally {
            plan.delete();
            generic.delete();
        }
        println("Exported " + entries.size() + " action-key lifetime helper identities");
    }
}
