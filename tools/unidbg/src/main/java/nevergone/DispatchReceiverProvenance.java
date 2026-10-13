package nevergone;

import com.alibaba.fastjson.JSONObject;
import com.github.unidbg.AndroidEmulator;
import com.github.unidbg.Module;
import com.github.unidbg.arm.context.RegisterContext;

/** Bounded receiver/vtable evidence captured at an already-observed ARM32 BLX call site. */
final class DispatchReceiverProvenance {
    private static final long LIBCOCOS_GHIDRA_BIAS = 0x10000L;

    static void capture(AndroidEmulator emulator, JSONObject row, JSONObject spec,
                        RegisterContext context, long rawTarget) {
        long receiver = Integer.toUnsignedLong(context.getIntArg(0));
        row.put("receiver_pointer", hex32(receiver));

        Long slotOffset = parseSlotOffset(spec.getString("slot_group_id"));
        row.put("dispatch_slot_offset", slotOffset == null ? null : String.format("0x%x", slotOffset));

        if (receiver == 0) {
            row.put("receiver_provenance_status", "null_receiver");
            return;
        }

        try {
            long vtable = readU32(emulator, receiver);
            row.put("receiver_vtable_raw", hex32(vtable));
            String vtableId = putPointerIdentity(emulator, row, "receiver_vtable", vtable);

            if (slotOffset == null) {
                row.put("receiver_provenance_status", "missing_slot_offset");
                return;
            }

            long slotAddress = (vtable + slotOffset) & 0xffffffffL;
            long slotTarget = readU32(emulator, slotAddress);
            boolean targetMatch = (slotTarget & ~1L) == (rawTarget & ~1L);

            row.put("receiver_slot_address", hex32(slotAddress));
            row.put("receiver_slot_target_raw", hex32(slotTarget));
            row.put("receiver_slot_target_thumb", (slotTarget & 1L) != 0);
            row.put("receiver_slot_matches_blx_target", targetMatch);
            row.put("receiver_dispatch_key",
                    vtableId + "@slot=" + String.format("0x%x", slotOffset));
            row.put("receiver_provenance_status",
                    targetMatch ? "verified_slot_match" : "slot_target_mismatch");
        } catch (Throwable error) {
            // Receiver evidence is advisory. Never discard the already-verified BLX target.
            row.put("receiver_provenance_status", "unavailable");
            row.put("receiver_provenance_error_type", error.getClass().getName());
            row.put("receiver_provenance_error", String.valueOf(error.getMessage()));
        }
    }

    private static long readU32(AndroidEmulator emulator, long address) {
        byte[] data = emulator.getBackend().mem_read(address, 4);
        if (data == null || data.length != 4)
            throw new IllegalStateException("Unable to read 4 bytes at " + hex32(address));
        return Integer.toUnsignedLong(
                (data[0] & 0xff)
                        | ((data[1] & 0xff) << 8)
                        | ((data[2] & 0xff) << 16)
                        | ((data[3] & 0xff) << 24));
    }

    private static Long parseSlotOffset(String slotGroupId) {
        if (slotGroupId == null) return null;
        int marker = slotGroupId.lastIndexOf("@0x");
        if (marker < 0) return null;
        String value = slotGroupId.substring(marker + 3);
        if (!value.matches("[0-9a-fA-F]+")) return null;
        return Long.parseLong(value, 16);
    }

    private static String putPointerIdentity(AndroidEmulator emulator, JSONObject row,
                                             String prefix, long pointer) {
        for (Module loaded : emulator.getMemory().getLoadedModules()) {
            if (pointer >= loaded.base && pointer < loaded.base + loaded.size) {
                long runtimeOffset = pointer - loaded.base;
                row.put(prefix + "_module", loaded.name);
                row.put(prefix + "_module_offset", String.format("0x%x", runtimeOffset));
                row.put(prefix + "_runtime_id", loaded.name + ":" + String.format("%08x", runtimeOffset));
                if ("libcocos2dcpp.so".equals(loaded.name)) {
                    long ghidraAddress = runtimeOffset + LIBCOCOS_GHIDRA_BIAS;
                    String id = loaded.name + ":" + String.format("%08x", ghidraAddress);
                    row.put(prefix + "_ghidra_address", String.format("0x%x", ghidraAddress));
                    row.put(prefix + "_id", id);
                    return id;
                }
                String id = loaded.name + ":" + String.format("%08x", runtimeOffset);
                row.put(prefix + "_ghidra_address", null);
                row.put(prefix + "_id", id);
                return id;
            }
        }
        String id = "absolute:" + hex32(pointer);
        row.put(prefix + "_module", null);
        row.put(prefix + "_module_offset", null);
        row.put(prefix + "_runtime_id", id);
        row.put(prefix + "_ghidra_address", null);
        row.put(prefix + "_id", id);
        return id;
    }

    private static String hex32(long value) {
        return String.format("0x%08x", value & 0xffffffffL);
    }

    private DispatchReceiverProvenance() {}
}
