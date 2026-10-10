package nevergone;

import com.alibaba.fastjson.JSONArray;
import com.alibaba.fastjson.JSONObject;
import com.github.unidbg.AndroidEmulator;
import com.github.unidbg.Module;
import com.github.unidbg.memory.MemoryBlock;
import com.github.unidbg.pointer.UnidbgPointer;

import java.util.Arrays;

/**
 * Bounded EnemyActionsData constructor probe.
 *
 * This deliberately calls only the zero-argument constructor. It does not call
 * initWithFile/loadWBGFile and therefore does not synthesize CCString,
 * CCFileUtils, HPData, CCArray or game filesystem state.
 */
final class EnemyActionsConstructorProbe {
    private static final String CONSTRUCTOR = "_ZN16EnemyActionsDataC1Ev";
    private static final long CONSTRUCTOR_OFFSET = 0x28f394L;
    private static final int OBJECT_BYTES = 0x400;
    private static final int GUARD_BYTES = 16;
    private static final byte SENTINEL = (byte) 0xa5;

    private static final int[] EXPECTED_ZERO_POINTER_OFFSETS = {
            0x14, 0x18, 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x30,
            0x34, 0x38, 0x3c, 0x40, 0x44, 0x48, 0x4c, 0x50,
            0x54, 0x58, 0x5c, 0x60, 0x64, 0x68, 0x6c, 0x70,
            0x74, 0x78, 0x7c, 0x80,
            0x84, 0x88, 0x8c, 0x90, 0x94, 0x98, 0x9c, 0xa0,
            0xc8, 0xcc,
    };

    private EnemyActionsConstructorProbe() {}

    static void run(AndroidEmulator emulator, Module module) {
        if (module.findSymbolByName(CONSTRUCTOR, false) == null)
            throw new IllegalStateException("Missing EnemyActionsData constructor symbol");
        long runtimeOffset = (module.findSymbolByName(CONSTRUCTOR, false).getAddress() - module.base) & ~1L;
        if (runtimeOffset != CONSTRUCTOR_OFFSET)
            throw new IllegalStateException(String.format(
                    "EnemyActionsData constructor offset mismatch: expected 0x%x got 0x%x",
                    CONSTRUCTOR_OFFSET, runtimeOffset));

        MemoryBlock block = emulator.getMemory().malloc(4096, true);
        try {
            UnidbgPointer object = block.getPointer();
            byte[] seeded = new byte[OBJECT_BYTES + GUARD_BYTES];
            Arrays.fill(seeded, SENTINEL);
            object.write(0, seeded, 0, seeded.length);

            byte[] before = object.getByteArray(0, OBJECT_BYTES);
            byte[] guardBefore = object.getByteArray(OBJECT_BYTES, GUARD_BYTES);
            Number result = module.callFunction(emulator, CONSTRUCTOR, object);
            byte[] after = object.getByteArray(0, OBJECT_BYTES);
            byte[] guardAfter = object.getByteArray(OBJECT_BYTES, GUARD_BYTES);

            if (!Arrays.equals(guardBefore, guardAfter))
                throw new IllegalStateException("EnemyActionsData constructor crossed 0x400-byte probe guard");

            JSONArray zeroSlots = new JSONArray();
            for (int offset : EXPECTED_ZERO_POINTER_OFFSETS) {
                int value = readLe32(after, offset);
                JSONObject slot = new JSONObject(true);
                slot.put("offset", String.format("0x%x", offset));
                slot.put("value", String.format("0x%08x", value));
                slot.put("zero", value == 0);
                zeroSlots.add(slot);
                if (value != 0)
                    throw new IllegalStateException(String.format(
                            "Expected constructor-zeroed pointer slot 0x%x is 0x%08x", offset, value));
            }

            JSONArray changedBytes = new JSONArray();
            for (int offset = 0; offset < OBJECT_BYTES; offset++) {
                if (before[offset] != after[offset]) changedBytes.add(offset);
            }

            JSONArray changedWords = new JSONArray();
            for (int offset = 0; offset < OBJECT_BYTES; offset += 4) {
                int beforeWord = readLe32(before, offset);
                int afterWord = readLe32(after, offset);
                if (beforeWord == afterWord) continue;
                JSONObject word = new JSONObject(true);
                word.put("offset", String.format("0x%x", offset));
                word.put("before", String.format("0x%08x", beforeWord));
                word.put("after", String.format("0x%08x", afterWord));
                changedWords.add(word);
            }

            JSONObject row = new JSONObject(true);
            row.put("schema", 1);
            row.put("kind", "enemy_actions_constructor");
            row.put("module", module.name);
            row.put("module_base", String.format("0x%x", module.base));
            row.put("symbol", CONSTRUCTOR);
            row.put("offset", String.format("0x%x", CONSTRUCTOR_OFFSET));
            row.put("arch", "ARM32 Thumb");
            row.put("object_bytes", OBJECT_BYTES);
            row.put("minimum_observed_object_bytes", 0x3f8);
            row.put("sentinel", SENTINEL & 255);
            row.put("return_value", result.longValue());
            row.put("changed_byte_offsets", changedBytes);
            row.put("changed_dwords", changedWords);
            row.put("expected_zero_pointer_slots", zeroSlots);
            row.put("guard_before_hex", EvidenceProbe.hex(guardBefore));
            row.put("guard_after_hex", EvidenceProbe.hex(guardAfter));
            JSONObject trace = new JSONObject(true);
            trace.put("mode", "none");
            trace.put("reason", "bounded constructor probe; no instruction hook");
            row.put("trace", trace);
            EvidenceProbe.emit(row);
        } finally {
            block.free();
        }
    }

    private static int readLe32(byte[] bytes, int offset) {
        return (bytes[offset] & 255)
                | ((bytes[offset + 1] & 255) << 8)
                | ((bytes[offset + 2] & 255) << 16)
                | ((bytes[offset + 3] & 255) << 24);
    }
}
