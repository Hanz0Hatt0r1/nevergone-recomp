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
import java.util.HashMap;
import java.util.List;
import java.util.Map;

// Nevergone adapter for the generic REVENGINSTR caller-argument exporter.
// Keeps the historical plan and TSV contract; the legacy exporter remains the
// fallback when the shared checkout is not on the Ghidra script path.
public class ExportCallerArgumentSubtree extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraCallerArgumentSubtree/1";
    private static final String LEGACY_HEADER = "format\trow_type\tsource_id\tsource_call_index\tsource_entry\tparameter_index\tdispatch_slot_offset\t" +
        "expected_direct_caller_functions\texpected_matched_caller_calls\tdirect_caller_functions\t" +
        "caller_functions_decompiled\tcaller_functions_failed\tcallers_with_matched_calls\tmatched_caller_calls\t" +
        "caller_entry\tcaller_name\tcaller_call_site\tnode_id\tparent_node_id\tinput_index\tdepth\tedge_kind\t" +
        "node_token\ttype_display\tpointer_depth\tpointee_type_display\tconstant_value\tdefining_opcode\t" +
        "defining_address\tdefining_time\tcall_target_entry\tcall_target_name\ttruncation\tstatus";
    private static final String GENERIC_HEADER = "format\trow_type\tplan_id\tcallee_entry\tparameter_index\tdirect_caller_functions\t" +
        "caller_functions_decompiled\tcaller_functions_failed\tcallers_with_matched_calls\tmatched_caller_calls\t" +
        "caller_entry\tcaller_name\tcaller_call_site\tnode_id\tparent_node_id\tinput_index\tdepth\tedge_kind\t" +
        "node_token\ttype_display\tpointer_depth\tpointee_type_display\tconstant_value\tdefining_opcode\t" +
        "defining_address\tdefining_time\tcall_target_entry\tcall_target_name\ttruncation\tstatus";

    private static class PlanRow {
        final String[] cells;
        PlanRow(String[] values) { cells = values; }
    }

    private String cell(String value) {
        return value.replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private List<PlanRow> readPlan(File file) throws Exception {
        ArrayList<PlanRow> rows = new ArrayList<PlanRow>();
        try (BufferedReader in = new BufferedReader(new InputStreamReader(new FileInputStream(file), StandardCharsets.UTF_8))) {
            String line;
            boolean first = true;
            while ((line = in.readLine()) != null) {
                if (line.trim().isEmpty()) continue;
                String[] values = line.split("\\t", -1);
                if (first && values.length >= 7 && "source_id".equals(values[0])) {
                    first = false;
                    continue;
                }
                first = false;
                if (values.length < 7) throw new IllegalArgumentException("Invalid plan row: " + line);
                Integer.parseInt(values[2]);
                if (Integer.parseInt(values[3]) < 0) throw new IllegalArgumentException("Negative parameter index: " + line);
                Integer.parseInt(values[5]);
                Integer.parseInt(values[6]);
                rows.add(new PlanRow(values));
            }
        }
        return rows;
    }

    private Map<String, Integer> headerIndex(String header, String expected) {
        if (!expected.equals(header)) throw new IllegalArgumentException("Unexpected shared caller subtree header");
        String[] names = header.split("\\t", -1);
        HashMap<String, Integer> indexes = new HashMap<String, Integer>();
        for (int i = 0; i < names.length; i++) indexes.put(names[i], i);
        return indexes;
    }

    private int number(String[] row, Map<String, Integer> indexes, String name) {
        return Integer.parseInt(row[indexes.get(name)]);
    }

    private void writeGenericPlan(File file, List<PlanRow> plan) throws Exception {
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(file), StandardCharsets.UTF_8))) {
            out.write("plan_id\tcallee_entry\tparameter_index");
            out.newLine();
            for (int i = 0; i < plan.size(); i++) {
                String[] c = plan.get(i).cells;
                out.write(i + "\t" + cell(c[1]) + "\t" + cell(c[3]));
                out.newLine();
            }
        }
    }

    private void writeLegacy(File genericOutput, File output, List<PlanRow> plan) throws Exception {
        try (BufferedReader in = new BufferedReader(new InputStreamReader(new FileInputStream(genericOutput), StandardCharsets.UTF_8));
             BufferedWriter out = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(output), StandardCharsets.UTF_8))) {
            String header = in.readLine();
            Map<String, Integer> indexes = headerIndex(header, GENERIC_HEADER);
            out.write(LEGACY_HEADER);
            out.newLine();
            String line;
            while ((line = in.readLine()) != null) {
                if (line.isEmpty()) continue;
                String[] generic = line.split("\\t", -1);
                if (generic.length != GENERIC_HEADER.split("\\t", -1).length)
                    throw new IllegalArgumentException("Invalid shared caller subtree row");
                int planIndex = Integer.parseInt(generic[indexes.get("plan_id")]);
                if (planIndex < 0 || planIndex >= plan.size()) throw new IllegalArgumentException("Unknown plan id: " + planIndex);
                String[] local = plan.get(planIndex).cells;
                String[] legacy = new String[34];
                legacy[0] = FORMAT;
                legacy[1] = generic[indexes.get("row_type")];
                legacy[2] = cell(local[0]);
                legacy[3] = cell(local[2]);
                legacy[4] = cell(local[1]);
                legacy[5] = cell(local[3]);
                legacy[6] = cell(local[4]);
                legacy[7] = cell(local[5]);
                legacy[8] = cell(local[6]);
                for (int i = 5; i < generic.length; i++) legacy[i + 4] = generic[i];
                if ("summary".equals(legacy[1]) && "complete_caller_argument_subtrees".equals(legacy[33]) &&
                    (number(generic, indexes, "direct_caller_functions") != Integer.parseInt(local[5]) ||
                     number(generic, indexes, "matched_caller_calls") != Integer.parseInt(local[6]))) {
                    legacy[33] = "caller_coverage_changed";
                }
                out.write(String.join("\t", legacy));
                out.newLine();
            }
        }
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2 && args.length != 3)
            throw new IllegalArgumentException("Usage: ExportCallerArgumentSubtree.java <plan.tsv> <output.tsv> [shared-ghidra-script-dir]");
        File output = new File(args[1]);
        File parent = output.getAbsoluteFile().getParentFile();
        if (parent != null) parent.mkdirs();

        File sharedScript = args.length == 3
            ? new File(args[2], "ExportGhidraCallerArgumentSubtree.java")
            : null;
        if (sharedScript == null || !sharedScript.isFile()) {
            runScript("ExportCallerArgumentSubtreeLegacy.java", new String[] {args[0], args[1]});
            return;
        }

        List<PlanRow> plan = readPlan(new File(args[0]));
        File genericPlan = File.createTempFile("caller-subtree-plan-", ".tsv", parent);
        File genericOutput = File.createTempFile("caller-subtree-output-", ".tsv", parent);
        try {
            writeGenericPlan(genericPlan, plan);
            runScript("ExportGhidraCallerArgumentSubtree.java", new String[] {genericPlan.getAbsolutePath(), genericOutput.getAbsolutePath()});
            writeLegacy(genericOutput, output, plan);
        } finally {
            genericPlan.delete();
            genericOutput.delete();
        }
        println("CALLER_ARGUMENT_SUBTREE_FORMAT=" + FORMAT);
        println("CALLER_ARGUMENT_SUBTREE_ROWS=" + plan.size());
        println("CALLER_ARGUMENT_SUBTREE_OUTPUT=" + output.getAbsolutePath());
        println("CALLER_ARGUMENT_SUBTREE_AFFECTS_ROUTING=no");
    }
}
