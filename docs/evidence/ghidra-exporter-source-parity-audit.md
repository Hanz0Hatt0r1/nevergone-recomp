# Shared Ghidra P-code exporter source audit

**Inspected:** 2026-10-17  
**Nevergone `main`:** `d057305b894029c573c57e3db75c9112ba725e67`  
**`revenginstr` `main`:** `a9bac53d5217bfec4d567c6adb29c00e0bd0b821`

This is a source comparison only. It does not contain program exports and does
not claim a new same-program Ghidra run.

| Exporter | `revenginstr` source SHA-256 | Nevergone source SHA-256 | Result |
|---|---|---|---|
| `ExportFunctionPcodeFlow.java` | `f85000bd35a7775435b6bc05c8df2cdc1da22268fb3d610c67f443a8fff6a0d5` | `f85000bd35a7775435b6bc05c8df2cdc1da22268fb3d610c67f443a8fff6a0d5` | Source files are byte-identical. |
| `ExportBoundedCallGraphPcodeFlow.java` | `ed24e4018e5a937d430f4e24a82a8fcb1bef7e9cac1294ae55161c27031edc4b` | `fcacdd7ca23cc94aebd617cb9cd8cc06869fc85e5c5a1b066ccceca2f02e248d` | Priority policy and its CLI configuration differ. |
| `ExportBoundedReverseCallGraphPcodeFlow.java` | `a2b926f8516a970099b98b2c4d89adc0d866ad3910343e2c24dd294c7f11bb99` | `36b01c54430d13a8ccd5bc0aa91766ace907e3e012dd967e8afab938b1b24592` | Priority policy and its CLI configuration differ. |

The bounded Nevergone sources rank title-specific function-name fragments in
the exporter. The shared exporters accept priority rules as invocation
arguments. Those choices can change which functions fit inside a traversal
budget, so source comparison alone cannot establish output parity. The first
same-program forward/reverse comparison under equivalent rules is recorded in
the related evidence below; expand the root/bounds fixture set before reducing
the local fallbacks.

`revenginstr-export-parity` was added in
[`revenginstr` PR #48](https://github.com/Hanz0Hatt0r1/revenginstr/pull/48).
The tool stores hashes and row comparison results without copying TSV rows or
filesystem paths. The first bounded same-program run is recorded below.

## Related same-program evidence

The first bounded forward/reverse output comparison is recorded in
[bounded P-code exporter output parity](ghidra-export-parity.md).
