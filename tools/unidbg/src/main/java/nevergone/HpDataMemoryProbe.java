package nevergone;

import com.alibaba.fastjson.JSONObject;
import com.github.unidbg.AndroidEmulator;
import com.github.unidbg.Module;
import com.github.unidbg.memory.MemoryBlock;
import com.github.unidbg.pointer.UnidbgPointer;

import java.util.Arrays;

/**
 * Bounded in-memory HPData ABI probe.
 *
 * This deliberately avoids createWithContentsOfFile(), CCFileUtils, WBG parsing
 * and autorelease. It constructs one HPData directly from controlled bytes,
 * exercises the five exported getBytes overloads, then runs the destructor.
 */
final class HpDataMemoryProbe {
    private static final String CONSTRUCTOR = "_ZN6HPDataC1EPKhm";
    private static final String GET_CHARS = "_ZNK6HPData8getBytesEPc7HPRange";
    private static final String GET_INT = "_ZNK6HPData8getBytesERi7HPRange";
    private static final String GET_UNSIGNED = "_ZNK6HPData8getBytesERj7HPRange";
    private static final String GET_FLOAT = "_ZNK6HPData8getBytesERf7HPRange";
    private static final String GET_BOOL = "_ZNK6HPData8getBytesERb7HPRange";
    private static final String DESTRUCTOR = "_ZN6HPDataD1Ev";

    private static final long CONSTRUCTOR_OFFSET = 0x2c64acL;
    private static final long GET_CHARS_OFFSET = 0x2c6568L;
    private static final long GET_INT_OFFSET = 0x2c658aL;
    private static final long GET_UNSIGNED_OFFSET = 0x2c65acL;
    private static final long GET_FLOAT_OFFSET = 0x2c65ceL;
    private static final long GET_BOOL_OFFSET = 0x2c65f0L;
    private static final long DESTRUCTOR_OFFSET = 0x2c6470L;

    private static final int OBJECT_BYTES = 0x1c;
    private static final int GUARD_BYTES = 16;
    private static final byte SENTINEL = (byte) 0xa5;

    private static final byte[] INPUT = {
            0x12, 0x34, 0x56, 0x78,             // int: 0x78563412
            0x00, 0x00, (byte) 0xc0, 0x3f,     // float: 1.5f
            (byte) 0xef, (byte) 0xcd, (byte) 0xab, (byte) 0x89,
            0x01,                               // bool: true
            'W', 'B', 'G',
    };

    private HpDataMemoryProbe() {}

    static void run(AndroidEmulator emulator, Module module) {
        verifySymbol(module, CONSTRUCTOR, CONSTRUCTOR_OFFSET);
        verifySymbol(module, GET_CHARS, GET_CHARS_OFFSET);
        verifySymbol(module, GET_INT, GET_INT_OFFSET);
        verifySymbol(module, GET_UNSIGNED, GET_UNSIGNED_OFFSET);
        verifySymbol(module, GET_FLOAT, GET_FLOAT_OFFSET);
        verifySymbol(module, GET_BOOL, GET_BOOL_OFFSET);
        verifySymbol(module, DESTRUCTOR, DESTRUCTOR_OFFSET);

        MemoryBlock objectBlock = emulator.getMemory().malloc(OBJECT_BYTES + GUARD_BYTES, true);
        MemoryBlock inputBlock = emulator.getMemory().malloc(INPUT.length, true);
        MemoryBlock outputBlock = emulator.getMemory().malloc(32, true);
        try {
            UnidbgPointer object = objectBlock.getPointer();
            UnidbgPointer input = inputBlock.getPointer();
            UnidbgPointer output = outputBlock.getPointer();

            byte[] seededObject = new byte[OBJECT_BYTES + GUARD_BYTES];
            Arrays.fill(seededObject, SENTINEL);
            object.write(0, seededObject, 0, seededObject.length);
            input.write(0, INPUT, 0, INPUT.length);
            byte[] seededOutput = new byte[32];
            Arrays.fill(seededOutput, (byte) 0xcc);
            output.write(0, seededOutput, 0, seededOutput.length);

            byte[] guardBefore = object.getByteArray(OBJECT_BYTES, GUARD_BYTES);
            module.callFunction(emulator, CONSTRUCTOR, object, input, INPUT.length);
            byte[] guardAfterCtor = object.getByteArray(OBJECT_BYTES, GUARD_BYTES);
            if (!Arrays.equals(guardBefore, guardAfterCtor))
                throw new IllegalStateException("HPData constructor crossed 0x1c-byte object guard");

            if (object.getInt(0x14) != INPUT.length)
                throw new IllegalStateException("HPData constructor length slot mismatch");
            long ownedAddress = object.getInt(0x18) & 0xffffffffL;
            UnidbgPointer owned = UnidbgPointer.pointer(emulator, ownedAddress);
            if (owned == null)
                throw new IllegalStateException("HPData constructor produced null owned buffer");
            byte[] ownedBytes = owned.getByteArray(0, INPUT.length + 1);
            if (ownedBytes[INPUT.length] != 0 ||
                    !Arrays.equals(INPUT, Arrays.copyOf(ownedBytes, INPUT.length))) {
                throw new IllegalStateException("HPData constructor copy/NUL contract mismatch");
            }

            // HPRange is passed by value as two ARM32 words: offset then byte length.
            module.callFunction(emulator, GET_INT, object, output.share(0), 0, 4);
            module.callFunction(emulator, GET_FLOAT, object, output.share(4), 4, 4);
            module.callFunction(emulator, GET_UNSIGNED, object, output.share(8), 8, 4);
            module.callFunction(emulator, GET_BOOL, object, output.share(12), 12, 1);
            module.callFunction(emulator, GET_CHARS, object, output.share(13), 13, 3);

            if (output.getInt(0) != 0x78563412)
                throw new IllegalStateException("HPData int HPRange read mismatch");
            if (output.getInt(4) != 0x3fc00000)
                throw new IllegalStateException("HPData float HPRange read mismatch");
            if ((output.getInt(8) & 0xffffffffL) != 0x89abcdefL)
                throw new IllegalStateException("HPData unsigned HPRange read mismatch");
            if (output.getByte(12) != 1)
                throw new IllegalStateException("HPData bool HPRange read mismatch");
            byte[] chars = output.getByteArray(13, 3);
            if (!Arrays.equals(chars, new byte[] {'W', 'B', 'G'}))
                throw new IllegalStateException("HPData char HPRange read mismatch");

            module.callFunction(emulator, DESTRUCTOR, object);
            byte[] guardAfterDtor = object.getByteArray(OBJECT_BYTES, GUARD_BYTES);
            if (!Arrays.equals(guardBefore, guardAfterDtor))
                throw new IllegalStateException("HPData destructor crossed 0x1c-byte object guard");
            if (object.getInt(0x14) != 0 || object.getInt(0x18) != 0)
                throw new IllegalStateException("HPData destructor did not clear length/buffer slots");

            JSONObject row = new JSONObject(true);
            row.put("schema", 1);
            row.put("kind", "hpdata_memory_abi");
            row.put("module", module.name);
            row.put("module_base", String.format("0x%x", module.base));
            row.put("arch", "ARM32 Thumb");
            row.put("object_bytes", OBJECT_BYTES);
            row.put("length_offset", "0x14");
            row.put("owned_buffer_offset", "0x18");
            row.put("input_hex", EvidenceProbe.hex(INPUT));
            row.put("owned_copy_hex", EvidenceProbe.hex(ownedBytes));
            row.put("int_value", String.format("0x%08x", output.getInt(0)));
            row.put("float_bits", String.format("0x%08x", output.getInt(4)));
            row.put("unsigned_value", String.format("0x%08x", output.getInt(8)));
            row.put("bool_value", output.getByte(12) != 0);
            row.put("chars", new String(chars));
            row.put("guard_before_hex", EvidenceProbe.hex(guardBefore));
            row.put("guard_after_constructor_hex", EvidenceProbe.hex(guardAfterCtor));
            row.put("guard_after_destructor_hex", EvidenceProbe.hex(guardAfterDtor));
            JSONObject trace = new JSONObject(true);
            trace.put("mode", "none");
            trace.put("reason", "bounded in-memory HPData ABI probe; no filesystem or WBG parser call");
            row.put("trace", trace);
            EvidenceProbe.emit(row);
        } finally {
            outputBlock.free();
            inputBlock.free();
            objectBlock.free();
        }
    }

    private static void verifySymbol(Module module, String symbol, long expectedOffset) {
        if (module.findSymbolByName(symbol, false) == null)
            throw new IllegalStateException("Missing symbol: " + symbol);
        long runtimeOffset = (module.findSymbolByName(symbol, false).getAddress() - module.base) & ~1L;
        if (runtimeOffset != expectedOffset)
            throw new IllegalStateException(String.format(
                    "%s offset mismatch: expected 0x%x got 0x%x",
                    symbol, expectedOffset, runtimeOffset));
    }
}
