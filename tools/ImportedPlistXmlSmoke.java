package org.nevergone.recomp;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

public final class ImportedPlistXmlSmoke {
    private static final String HEADER =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    private static final String APPLE =
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" " +
            "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\"";
    private static final String BODY = "<plist version=\"1.0\"><dict/></plist>";

    private static boolean accepts(File file, String xml) throws Exception {
        Files.writeString(file.toPath(), xml, StandardCharsets.UTF_8);
        return ImportedPlistXml.parse(file, 4096) != null;
    }

    public static void main(String[] args) throws Exception {
        if (args.length == 1) {
            assert ImportedPlistXml.parse(new File(args[0]), 4L * 1024L * 1024L) != null;
        }
        File file = Files.createTempFile("nevergone-plist-xml", ".plist").toFile();
        try {
            assert accepts(file, HEADER + BODY);
            assert accepts(file, HEADER + APPLE + ">\n" + BODY);
            assert accepts(file, HEADER + APPLE + "/>\n" + BODY);
            assert !accepts(file, HEADER + "<!DOCTYPE plist [<!ENTITY x SYSTEM " +
                    "\"file:///etc/passwd\">]>" + BODY);
            assert !accepts(file, HEADER + APPLE + ">\n" + APPLE + ">\n" + BODY);
            Files.writeString(file.toPath(), HEADER + BODY, StandardCharsets.UTF_8);
            assert ImportedPlistXml.parse(file, 8) == null;
        } finally {
            Files.deleteIfExists(file.toPath());
        }
    }
}
