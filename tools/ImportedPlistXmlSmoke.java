package org.nevergone.recomp;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;

public final class ImportedPlistXmlSmoke {
    private static final String HEADER =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    private static final String APPLE =
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" " +
            "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\"";
    private static final String APPLE_COMPUTER =
            "<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" " +
            "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\"";
    private static final String BODY = "<plist version=\"1.0\"><dict/></plist>";

    private static final class NoXIncludeFactory extends DocumentBuilderFactory {
        boolean xincludeAttempted = false;

        @Override
        public DocumentBuilder newDocumentBuilder() throws ParserConfigurationException {
            throw new ParserConfigurationException("unused smoke factory");
        }

        @Override
        public void setAttribute(String name, Object value) {}

        @Override
        public Object getAttribute(String name) {
            return null;
        }

        @Override
        public void setFeature(String name, boolean value) {}

        @Override
        public boolean getFeature(String name) {
            return false;
        }

        @Override
        public void setXIncludeAware(boolean state) {
            assert !state;
            xincludeAttempted = true;
            throw new UnsupportedOperationException("XInclude unavailable");
        }
    }

    private static boolean accepts(File file, String xml) throws Exception {
        Files.writeString(file.toPath(), xml, StandardCharsets.UTF_8);
        return ImportedPlistXml.parse(file, 4096) != null;
    }

    public static void main(String[] args) throws Exception {
        if (args.length == 1) {
            assert ImportedPlistXml.parse(new File(args[0]), 4L * 1024L * 1024L) != null;
        }

        NoXIncludeFactory noXInclude = new NoXIncludeFactory();
        ImportedPlistXml.configureFactory(noXInclude);
        assert noXInclude.xincludeAttempted;

        File file = Files.createTempFile("nevergone-plist-xml", ".plist").toFile();
        try {
            assert accepts(file, HEADER + BODY);
            assert accepts(file, HEADER + APPLE + ">\n" + BODY);
            assert accepts(file, HEADER + APPLE + "/>\n" + BODY);
            assert accepts(file, HEADER + APPLE_COMPUTER + ">\n" + BODY);
            assert accepts(file, HEADER + APPLE_COMPUTER + "/>\n" + BODY);
            assert !accepts(file, HEADER + "<!DOCTYPE plist [<!ENTITY x SYSTEM " +
                    "\"file:///etc/passwd\">]>" + BODY);
            assert !accepts(file, HEADER + APPLE + ">\n" + APPLE + ">\n" + BODY);
            assert !accepts(file, HEADER + APPLE_COMPUTER + ">\n" + APPLE + ">\n" + BODY);
            Files.writeString(file.toPath(), HEADER + BODY, StandardCharsets.UTF_8);
            assert ImportedPlistXml.parse(file, 8) == null;
        } finally {
            Files.deleteIfExists(file.toPath());
        }
    }
}
