import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.LinkedHashSet;
import java.util.Set;

// Nevergone adapter: enrich generic xref rows with local-only nearby class
// evidence and retain the historical TSV contract. The original exporter is
// kept as the fallback.
public class ExportAddressReferenceOwners extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraAddressReferenceOwners/1";
    private static final String GENERIC_FORMAT = "REVENGINSTR.GhidraAddressReferenceOwner/1";
    private static final String GENERIC_HEADER = "format\ttarget_address\ttarget_block\ttarget_symbol\t" +
        "reference_from\treference_type\treference_source\toperand_index\tfrom_symbol\t" +
        "from_instruction\tcontaining_function_entry\tcontaining_function_name";
    private static final String LEGACY_HEADER = GENERIC_HEADER + "\tnearby_class_evidence";
    private static final int MAX_NEARBY_EVIDENCE = 32;

    private String clean(String value) {
        if (value == null || value.isEmpty()) return "-";
        return value.replace('\t', ' ').replace('\r', ' ').replace('\n', ' ');
    }

    private String symbolAt(Address address) {
        Symbol symbol = currentProgram.getSymbolTable().getPrimarySymbol(address);
        return symbol == null ? "-" : clean(symbol.getName(true));
    }

    private String nearbyClassEvidence(Function function) {
        Set<String> evidence = new LinkedHashSet<String>();
        Listing listing = currentProgram.getListing();
        for (Instruction instruction : listing.getInstructions(function.getBody(), true)) {
            if (evidence.size() >= MAX_NEARBY_EVIDENCE) break;
            for (Reference reference : instruction.getReferencesFrom()) {
                Address target = reference.getToAddress();
                if (target == null) continue;
                Function called = currentProgram.getFunctionManager().getFunctionAt(target);
                String name = called != null ? called.getName(true) : symbolAt(target);
                if (name == null || name.equals("-")) continue;
                String lower = name.toLowerCase();
                if (name.contains("::") || lower.contains("default_instance") ||
                        lower.contains("descriptor") || lower.contains("protobuf")) {
                    evidence.add(name);
                    if (evidence.size() >= MAX_NEARBY_EVIDENCE) break;
                }
            }
        }
        return evidence.isEmpty() ? "-" : clean(String.join(";", evidence));
    }

    private String nearbyForEntry(String entry) throws Exception {
        if (entry == null || entry.isEmpty() || entry.equals("-")) return "-";
        Address address = toAddr(entry);
        Function function = currentProgram.getFunctionManager().getFunctionAt(address);
        if (function == null) function = currentProgram.getFunctionManager().getFunctionContaining(address);
        return function == null ? "-" : nearbyClassEvidence(function);
    }

    private void writeLegacy(File generic, File output) throws Exception {
        try (BufferedReader in = new BufferedReader(new InputStreamReader(
                    new FileInputStream(generic), StandardCharsets.UTF_8));
             BufferedWriter out = new BufferedWriter(new OutputStreamWriter(
                    new FileOutputStream(output), StandardCharsets.UTF_8))) {
            if (!GENERIC_HEADER.equals(in.readLine())) {
                throw new IllegalArgumentException("Unexpected shared address-reference header");
            }
            out.write(LEGACY_HEADER);
            out.newLine();
            String line;
            while ((line = in.readLine()) != null) {
                if (line.isEmpty()) continue;
                String[] row = line.split("\\t", -1);
                if (row.length != 12 || !GENERIC_FORMAT.equals(row[0])) {
                    throw new IllegalArgumentException("Invalid shared address-reference row");
                }
                String[] legacy = Arrays.copyOf(row, 13);
                legacy[0] = FORMAT;
                legacy[12] = nearbyForEntry(row[10]);
                out.write(String.join("\t", legacy));
                out.newLine();
            }
        }
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        int targetEnd = args.length;
        File shared = null;
        if (args.length >= 4 && "--shared-ghidra-script-dir".equals(args[args.length - 2])) {
            shared = new File(args[args.length - 1], "ExportGhidraAddressReferenceOwners.java");
            targetEnd -= 2;
        }
        if (targetEnd < 2) {
            throw new IllegalArgumentException(
                "Usage: ExportAddressReferenceOwners.java <output.tsv> <address> [address ...] " +
                "[--shared-ghidra-script-dir <path>]");
        }
        File output = new File(args[0]);
        File parent = output.getAbsoluteFile().getParentFile();
        if (parent != null) parent.mkdirs();

        if (shared == null || !shared.isFile()) {
            runScript("ExportAddressReferenceOwnersLegacy.java", Arrays.copyOf(args, targetEnd));
            return;
        }

        File generic = File.createTempFile("address-reference-owners-", ".tsv", parent);
        try {
            String[] genericArgs = new String[targetEnd];
            genericArgs[0] = generic.getAbsolutePath();
            System.arraycopy(args, 1, genericArgs, 1, targetEnd - 1);
            runScript("ExportGhidraAddressReferenceOwners.java", genericArgs);
            writeLegacy(generic, output);
        } finally {
            generic.delete();
        }
        println("ADDRESS_REFERENCE_OWNERS_FORMAT=" + FORMAT);
        println("ADDRESS_REFERENCE_OWNERS_OUTPUT=" + output.getAbsolutePath());
        println("ADDRESS_REFERENCE_OWNERS_AFFECTS_ROUTING=no");
    }
}
