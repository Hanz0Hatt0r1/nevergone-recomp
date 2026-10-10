package nevergone;

import com.alibaba.fastjson.JSONObject;
import com.github.unidbg.AndroidEmulator;
import com.github.unidbg.Module;
import com.github.unidbg.memory.MemoryBlock;
import com.github.unidbg.pointer.UnidbgPointer;

import java.nio.charset.StandardCharsets;
import java.util.Arrays;

/**
 * Bounded cocos2d::CCString ABI probe used as a prerequisite for a future
 * EnemyActionsData::loadWBGFile call.
 *
 * The probe constructs one CCString from a plain C string, reads it back through
 * getCString(), then destroys it. It deliberately does not call initWithFile or
 * loadWBGFile and does not establish any filesystem, HPData or CCArray state.
 */
final class EnemyActionsCcStringProbe {
    private static final String CONSTRUCTOR = "_ZN7cocos2d8CCStringC1EPKc";
    private static final String GET_CSTRING = "_ZNK7cocos2d8CCString10getCStringEv";
    private static final String DESTRUCTOR = "_ZN7cocos2d8CCStringD1Ev";

    private static final long CONSTRUCTOR_OFFSET = 0x519db4L;
    private static final long GET_CSTRING_OFFSET = 0x519e8eL;
    private static final long DESTRUCTOR_OFFSET = 0x519d04L;

    // copyWithZone allocates 0x18 bytes for a CCString in the original binary,
    // and the constructor stores its std::string payload at object + 0x14.
    private static final int OBJECT_BYTES = 0x18;
    private static final int GUARD_BYTES = 16;
    private static final byte SENTINEL = (byte) 0xa5;
    private static final String INPUT = "enemy-actions-probe-missing.wbg";

    private EnemyActionsCcStringProbe() {}

    static void run(AndroidEmulator emulator, Module module) {
        verifySymbol(module, CONSTRUCTOR, CONSTRUCTOR_OFFSET);
        verifySymbol(module, GET_CSTRING, GET_CSTRING_OFFSET);
        verifySymbol(module, DESTRUCTOR, DESTRUCTOR_OFFSET);

        byte[] inputBytes = INPUT.getBytes(StandardCharsets.UTF_8);
        MemoryBlock objectBlock = emulator.getMemory().malloc(OBJECT_BYTES + GUARD_BYTES, true);
        MemoryBlock inputBlock = emulator.getMemory().malloc(inputBytes.length + 1, true);
        try {
            UnidbgPointer object = objectBlock.getPointer();
            UnidbgPointer input = inputBlock.getPointer();

            byte[] seeded = new byte[OBJECT_BYTES + GUARD_BYTES];
            Arrays.fill(seeded, SENTINEL);
            object.write(0, seeded, 0, seeded.length);
            input.write(0, inputBytes, 0, inputBytes.length);
            input.setByte(inputBytes.length, (byte) 0);

            byte[] guardBefore = object.getByteArray(OBJECT_BYTES, GUARD_BYTES);
            Number ctorResult = module.callFunction(emulator, CONSTRUCTOR, object, input);
            byte[] guardAfterCtor = object.getByteArray(OBJECT_BYTES, GUARD_BYTES);
            if (!Arrays.equals(guardBefore, guardAfterCtor))
                throw new IllegalStateException("CCString constructor crossed 0x18-byte object guard");

            Number rawCString = module.callFunction(emulator, GET_CSTRING, object);
            UnidbgPointer cString = UnidbgPointer.pointer(emulator, rawCString.longValue());
            if (cString == null)
                throw new IllegalStateException("CCString::getCString returned null");
            byte[] roundTripBytes = cString.getByteArray(0, inputBytes.length + 1);
            if (roundTripBytes[inputBytes.length] != 0 ||
                    !Arrays.equals(inputBytes, Arrays.copyOf(roundTripBytes, inputBytes.length))) {
                throw new IllegalStateException("CCString::getCString byte round-trip mismatch");
            }
            String roundTrip = new String(roundTripBytes, 0, inputBytes.length, StandardCharsets.UTF_8);

            // The accessor is proven by disassembly to load object+0x14 directly.
            long payloadPointer = object.getInt(0x14) & 0xffffffffL;
            if (payloadPointer != (rawCString.longValue() & 0xffffffffL))
                throw new IllegalStateException(String.format(
                        "CCString payload pointer mismatch: slot=0x%x accessor=0x%x",
                        payloadPointer, rawCString.longValue() & 0xffffffffL));

            Number dtorResult = module.callFunction(emulator, DESTRUCTOR, object);
            byte[] guardAfterDtor = object.getByteArray(OBJECT_BYTES, GUARD_BYTES);
            if (!Arrays.equals(guardBefore, guardAfterDtor))
                throw new IllegalStateException("CCString destructor crossed 0x18-byte object guard");

            JSONObject row = new JSONObject(true);
            row.put("schema", 1);
            row.put("kind", "enemy_actions_ccstring_prerequisite");
            row.put("module", module.name);
            row.put("module_base", String.format("0x%x", module.base));
            row.put("arch", "ARM32 Thumb");
            row.put("constructor_symbol", CONSTRUCTOR);
            row.put("constructor_offset", String.format("0x%x", CONSTRUCTOR_OFFSET));
            row.put("get_cstring_symbol", GET_CSTRING);
            row.put("get_cstring_offset", String.format("0x%x", GET_CSTRING_OFFSET));
            row.put("destructor_symbol", DESTRUCTOR);
            row.put("destructor_offset", String.format("0x%x", DESTRUCTOR_OFFSET));
            row.put("object_bytes", OBJECT_BYTES);
            row.put("payload_pointer_offset", "0x14");
            row.put("input", INPUT);
            row.put("round_trip", roundTrip);
            row.put("constructor_return", String.format("0x%x", ctorResult.longValue() & 0xffffffffL));
            row.put("get_cstring_return", String.format("0x%x", rawCString.longValue() & 0xffffffffL));
            row.put("destructor_return", String.format("0x%x", dtorResult.longValue() & 0xffffffffL));
            row.put("guard_before_hex", EvidenceProbe.hex(guardBefore));
            row.put("guard_after_constructor_hex", EvidenceProbe.hex(guardAfterCtor));
            row.put("guard_after_destructor_hex", EvidenceProbe.hex(guardAfterDtor));
            JSONObject trace = new JSONObject(true);
            trace.put("mode", "none");
            trace.put("reason", "bounded CCString prerequisite probe; no instruction hook");
            row.put("trace", trace);
            EvidenceProbe.emit(row);
        } finally {
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
