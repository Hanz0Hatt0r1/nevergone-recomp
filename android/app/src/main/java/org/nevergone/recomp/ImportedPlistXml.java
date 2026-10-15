package org.nevergone.recomp;

import org.w3c.dom.Document;
import org.xml.sax.InputSource;

import java.io.File;
import java.io.StringReader;
import java.nio.ByteBuffer;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;

/** Parses bounded imported plists while accepting the shipped Apple header. */
final class ImportedPlistXml {
    private static final String APPLE_DTD =
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" " +
            "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">";
    // Older shipped scene atlases use Apple's earlier public identifier.
    private static final String APPLE_COMPUTER_DTD =
            "<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" " +
            "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">";
    // TexturePacker's shipped player atlases contain this non-XML closing '/>'.
    private static final String APPLE_DTD_TEXTURE_PACKER =
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" " +
            "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\"/>";
    private static final String APPLE_COMPUTER_DTD_TEXTURE_PACKER =
            "<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" " +
            "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\"/>";

    private ImportedPlistXml() {}

    static Document parse(File file, long maxBytes) throws Exception {
        if (file == null || maxBytes <= 0 || !file.isFile() ||
                file.length() <= 0 || file.length() > maxBytes) return null;
        byte[] bytes = Files.readAllBytes(file.toPath());
        if (bytes.length == 0 || bytes.length > maxBytes) return null;
        String xml = StandardCharsets.UTF_8.newDecoder()
                .onMalformedInput(CodingErrorAction.REPORT)
                .onUnmappableCharacter(CodingErrorAction.REPORT)
                .decode(ByteBuffer.wrap(bytes)).toString();
        int dtd = xml.indexOf("<!DOCTYPE");
        if (dtd >= 0) {
            String expected = xml.startsWith(APPLE_DTD, dtd)
                    ? APPLE_DTD
                    : xml.startsWith(APPLE_COMPUTER_DTD, dtd)
                            ? APPLE_COMPUTER_DTD
                            : xml.startsWith(APPLE_DTD_TEXTURE_PACKER, dtd)
                                    ? APPLE_DTD_TEXTURE_PACKER
                                    : xml.startsWith(APPLE_COMPUTER_DTD_TEXTURE_PACKER, dtd)
                                            ? APPLE_COMPUTER_DTD_TEXTURE_PACKER : null;
            if (expected == null || dtd > 512) return null;
            xml = xml.substring(0, dtd) + xml.substring(dtd + expected.length());
        }
        if (xml.contains("<!DOCTYPE")) return null;

        DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
        configureFactory(factory);
        DocumentBuilder builder = factory.newDocumentBuilder();
        builder.setEntityResolver((publicId, systemId) -> new InputSource(new StringReader("")));
        return builder.parse(new InputSource(new StringReader(xml)));
    }

    static void configureFactory(DocumentBuilderFactory factory) {
        factory.setNamespaceAware(false);
        try {
            factory.setXIncludeAware(false);
        } catch (UnsupportedOperationException ignored) {
            // Some Android/libcore JAXP implementations reject this API even
            // for false. XInclude defaults to disabled, so continuing preserves
            // the intended policy while retaining compatibility with the device
            // parser used for imported TexturePacker plists.
        }
        factory.setExpandEntityReferences(false);
        trySetFeature(factory, "http://apache.org/xml/features/disallow-doctype-decl", true);
        trySetFeature(factory, "http://xml.org/sax/features/external-general-entities", false);
        trySetFeature(factory, "http://xml.org/sax/features/external-parameter-entities", false);
        trySetFeature(factory, "http://apache.org/xml/features/nonvalidating/load-external-dtd", false);
    }

    private static void trySetFeature(DocumentBuilderFactory factory, String name, boolean value) {
        try {
            factory.setFeature(name, value);
        } catch (Exception ignored) {
            // The exact Apple declaration has already been removed; the
            // resolver also refuses any external system identifier.
        }
    }
}
