import ghidra.app.script.GhidraScript;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;

// Nevergone adapter: use revenginstr's defined-string exporter while keeping
// the historical TSV format. The original exporter remains the fallback.
public class ExportPcodeDefinedStrings extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraPcodeDefinedString/1";
    private static final String GENERIC_FORMAT = "REVENGINSTR.GhidraPcodeDefinedString/1";
    private static final String HEADER =
        "format\tconstant_address\tdata_address\tbyte_offset\tutf8_base64\tlength";

    private void writeLegacy(File generic, File output) throws Exception {
        try (BufferedReader in = new BufferedReader(new InputStreamReader(
                    new FileInputStream(generic), StandardCharsets.UTF_8));
             BufferedWriter out = new BufferedWriter(new OutputStreamWriter(
                    new FileOutputStream(output), StandardCharsets.UTF_8))) {
            if (!HEADER.equals(in.readLine())) {
                throw new IllegalArgumentException("Unexpected shared defined-string header");
            }
            out.write(HEADER);
            out.newLine();
            String line;
            while ((line = in.readLine()) != null) {
                if (line.isEmpty()) continue;
                String[] values = line.split("\\t", -1);
                if (values.length != 6 || !GENERIC_FORMAT.equals(values[0])) {
                    throw new IllegalArgumentException("Invalid shared defined-string row");
                }
                values[0] = FORMAT;
                out.write(String.join("\t", values));
                out.newLine();
            }
        }
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2 && args.length != 3) {
            throw new IllegalArgumentException(
                "Usage: ExportPcodeDefinedStrings.java <pcode.tsv> <output.tsv> [shared-ghidra-script-dir]");
        }
        File output = new File(args[1]);
        File parent = output.getAbsoluteFile().getParentFile();
        if (parent != null) parent.mkdirs();

        File shared = args.length == 3
            ? new File(args[2], "ExportGhidraPcodeDefinedStrings.java")
            : null;
        if (shared == null || !shared.isFile()) {
            runScript("ExportPcodeDefinedStringsLegacy.java", new String[] {args[0], args[1]});
            return;
        }

        File generic = File.createTempFile("defined-strings-", ".tsv", parent);
        try {
            runScript("ExportGhidraPcodeDefinedStrings.java", new String[] {
                args[0], generic.getAbsolutePath()
            });
            writeLegacy(generic, output);
        } finally {
            generic.delete();
        }
        println("Exported generic defined strings in the historical Nevergone TSV format -> " + output);
    }
}
