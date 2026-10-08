import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.symbol.Reference;
import java.io.BufferedWriter;
import java.io.FileWriter;
import java.util.regex.Pattern;

public class NevergoneIndex extends GhidraScript {
    public void run() throws Exception {
        String out = getScriptArgs()[0];
        Pattern focus = Pattern.compile("(?i)SingleLogin|HelloWorld|zjmyun|zjmdengguang|zjmshandian|zjmjianzhuzhaoliang|LoginScreen|createUI|Func0[12]");
        try (BufferedWriter w = new BufferedWriter(new FileWriter(out))) {
            w.write("type\taddress\tvalue\treferences\n");
            FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
            for (Function f : functions) {
                if (monitor.isCancelled()) break;
                String name = f.getName(true);
                if (!focus.matcher(name).find()) continue;
                StringBuilder refs = new StringBuilder();
                for (Reference r : getReferencesTo(f.getEntryPoint())) {
                    if (refs.length() > 0) refs.append(',');
                    refs.append(r.getFromAddress());
                }
                w.write("function\t" + f.getEntryPoint() + "\t" + clean(name) + "\t" + references(refs) + "\n");
            }
            DataIterator data = currentProgram.getListing().getDefinedData(true);
            for (Data d : data) {
                if (monitor.isCancelled()) break;
                if (!(d.getValue() instanceof String)) continue;
                String value = (String)d.getValue();
                if (!focus.matcher(value).find()) continue;
                StringBuilder refs = new StringBuilder();
                for (Reference r : getReferencesTo(d.getAddress())) {
                    if (refs.length() > 0) refs.append(',');
                    refs.append(r.getFromAddress());
                }
                w.write("string\t" + d.getAddress() + "\t" + clean(value) + "\t" + references(refs) + "\n");
            }
        }
    }
    private String clean(String s) { return s.replace('\t', ' ').replace('\r', ' ').replace('\n', ' '); }
    private String references(StringBuilder refs) { return refs.length() == 0 ? "-" : refs.toString(); }
}
