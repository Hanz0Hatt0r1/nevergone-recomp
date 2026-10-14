# CCJoyStick field factory provenance

This evidence step follows the one remaining non-quarantined `receiver_not_single_pointer` row after v12:

- source `libcocos2dcpp.so:002a647e`, call 4, slot `0x58`;
- exact Ghidra function identity `CCJoyStick::setStickTexture`;
- receiver address proven as `parameter0 + 0x104`.

The probe does **not** infer a receiver class from the owner name or field offset alone.

Instead it decompiles exact `CCJoyStick::init` at Ghidra entry `0x002a6584`, inventories parameter-relative STOREs, and selects only an exact `this + 0x104` STORE whose stored SSA root is a direct CALL. That callee is then passed to the pre-existing `ExportFunctionReturnObjectProvenance` / `CallRootFactoryReturnAudit` constructor-return proof. Finally, the constructor-proven class and unique vtable are checked against `StaticVtableSlotCatalog/3` at slot `0x58`.

Acceptance requires all of these independent gates:

1. current parameter-relative owner audit remains accepted for `CCJoyStick +0x104`;
2. exactly one `CCJoyStick::init` STORE writes `this +0x104` from a direct CALL root;
3. the factory's every non-null return leaf is constructor-proven to one catalog class;
4. that class maps to one vtable;
5. the exact vtable/slot `0x58` catalog key exists once and resolves to one function-memory target.

The probe is evidence-only and emits no receiver query. `affects_routing=false`.

Run:

```bash
bash tools/ml/run_nevergone_ccjoystick_field_factory_provenance.sh
```
