import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;

import java.io.BufferedWriter;
import java.io.FileWriter;
import java.util.LinkedHashSet;
import java.util.Set;

public class ExportAddressReferenceOwnersLegacy extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraAddressReferenceOwners/1";
    private static final int MAX_NEARBY_EVIDENCE = 32;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            throw new IllegalArgumentException("usage: ExportAddressReferenceOwners.java <out.tsv> <address> [address ...]");
        }

        Listing listing = currentProgram.getListing();
        try (BufferedWriter out = new BufferedWriter(new FileWriter(args[0]))) {
            out.write("format\ttarget_address\ttarget_block\ttarget_symbol\treference_from\treference_type\treference_source\toperand_index\tfrom_symbol\tfrom_instruction\tcontaining_function_entry\tcontaining_function_name\tnearby_class_evidence\n");

            for (int i = 1; i < args.length; i++) {
                monitor.checkCancelled();
                Address target = toAddr(args[i]);
                MemoryBlock block = currentProgram.getMemory().getBlock(target);
                String targetBlock = block == null ? "-" : block.getName();
                String targetSymbol = symbolAt(target);

                ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(target);
                boolean any = false;
                while (refs.hasNext()) {
                    monitor.checkCancelled();
                    any = true;
                    Reference ref = refs.next();
                    Address from = ref.getFromAddress();
                    Function containing = currentProgram.getFunctionManager().getFunctionContaining(from);
                    Instruction ins = listing.getInstructionAt(from);
                    String fromSymbol = symbolAt(from);
                    String instruction = ins == null ? "-" : ins.getMnemonicString();
                    String fnEntry = containing == null ? "-" : containing.getEntryPoint().toString();
                    String fnName = containing == null ? "-" : containing.getName(true);
                    String nearby = containing == null ? "-" : nearbyClassEvidence(containing);

                    writeRow(out,
                        target.toString(), targetBlock, targetSymbol,
                        from.toString(), ref.getReferenceType().toString(), ref.getSource().toString(),
                        Integer.toString(ref.getOperandIndex()), fromSymbol, instruction,
                        fnEntry, fnName, nearby);
                }

                if (!any) {
                    writeRow(out, target.toString(), targetBlock, targetSymbol,
                        "-", "-", "-", "-", "-", "-", "-", "-", "-");
                }
            }
        }

        println("ADDRESS_REFERENCE_OWNERS_FORMAT=" + FORMAT);
        println("ADDRESS_REFERENCE_OWNERS_OUTPUT=" + args[0]);
        println("ADDRESS_REFERENCE_OWNERS_AFFECTS_ROUTING=no");
    }

    private String nearbyClassEvidence(Function function) {
        Set<String> evidence = new LinkedHashSet<String>();
        Listing listing = currentProgram.getListing();
        for (Instruction ins : listing.getInstructions(function.getBody(), true)) {
            if (evidence.size() >= MAX_NEARBY_EVIDENCE) {
                break;
            }
            for (Reference ref : ins.getReferencesFrom()) {
                Address to = ref.getToAddress();
                if (to == null) {
                    continue;
                }
                Function called = currentProgram.getFunctionManager().getFunctionAt(to);
                String name = called != null ? called.getName(true) : symbolAt(to);
                if (name == null || name.equals("-")) {
                    continue;
                }
                String lower = name.toLowerCase();
                if (name.contains("::") || lower.contains("default_instance") || lower.contains("descriptor") || lower.contains("protobuf")) {
                    evidence.add(name);
                    if (evidence.size() >= MAX_NEARBY_EVIDENCE) {
                        break;
                    }
                }
            }
        }
        if (evidence.isEmpty()) {
            return "-";
        }
        return clean(String.join(";", evidence));
    }

    private String symbolAt(Address address) {
        Symbol symbol = currentProgram.getSymbolTable().getPrimarySymbol(address);
        return symbol == null ? "-" : clean(symbol.getName(true));
    }

    private void writeRow(BufferedWriter out, String... fields) throws Exception {
        out.write(FORMAT);
        for (String field : fields) {
            out.write('\t');
            out.write(clean(field));
        }
        out.write('\n');
    }

    private String clean(String value) {
        if (value == null || value.isEmpty()) {
            return "-";
        }
        return value.replace('\t', ' ').replace('\r', ' ').replace('\n', ' ');
    }
}
