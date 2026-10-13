# Exact receiver-vtable slot resolution

`tools/ml/resolve_nevergone_receiver_vtable_slots.py` joins dispatch evidence only by the exact pair:

`(receiver_vtable_id, dispatch_slot_offset)`

This is deliberately stricter than structural slot equivalence. Runtime evidence already showed that the same structural slot shape and offset can dispatch to different implementations for different concrete receiver vtables.

The resolver therefore:

- never transfers targets across receiver vtables;
- never transfers targets across classes;
- preserves ambiguous static slot targets as ambiguous;
- blocks catalog rows already marked `runtime_mismatch`;
- distinguishes static-only single candidates from `runtime_match` rows;
- emits evidence only and does not change production routing.

Output format: `NEVERGONE.ReceiverVtableSlotResolution/1`.

A static-only exact-key result is still candidate evidence. A row is marked runtime-verified only when the input static catalog has already validated that exact receiver-vtable + slot target against runtime evidence.
