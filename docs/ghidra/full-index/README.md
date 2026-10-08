# Full metadata index for future recompilation work

This bundle is derived from the user's local Nevergone Ghidra project and original APK/OBB. It is an index for finding code and resources, not a decompilation. It contains no original binary, image, audio, Lua source, archive member content, or Ghidra pseudocode.

## Native programs

For each of `libcocos2dcpp` and `libffmpeg`, [ExportProgramIndex.java](../../../tools/ghidra/ExportProgramIndex.java) writes five gzip-compressed UTF-8 TSV files:

| File suffix | Contents |
| --- | --- |
| `functions.tsv.gz` | Function entry address, qualified name, body address count, external flag, Ghidra signature |
| `symbols.tsv.gz` | Symbol address, qualified name, type, source, primary flag |
| `strings.tsv.gz` | Defined string address, exact UTF-8 value encoded as Base64, Java string length |
| `xrefs.tsv.gz` | Every reference currently recorded by Ghidra: from/to addresses, type, operand, source |
| `calls.tsv.gz` | Resolved call edges with caller entry, call site, and callee entry |

Addresses use the Ghidra program's address space. For `libcocos2dcpp.so`, Ghidra loaded the program at `ELF virtual address + 0x10000`. Call and reference coverage is limited to what the analyzed project recognizes; indirect calls and unresolved references do not appear as resolved edges.

`native_manifest.json` records the SHA-256 of each original `.so` member and each exported table, plus row counts. The `libffmpeg.so` export now comes from a completed Ghidra auto-analysis in a temporary project copy. The first pass hit a 600-second limit and saved its progress; a second pass finished successfully in 307 seconds. A separate read-only export of the saved project produced byte-identical metadata files. The original user project was not modified. The completed analysis records 65,958 resolved call edges, while indirect and unresolved calls remain outside that graph.

## Archives and atlases

[index_archive_metadata.py](../../../tools/ghidra/index_archive_metadata.py) reads the user-owned APK and OBB and writes:

| File | Contents |
| --- | --- |
| `archive_manifest.json` | Source archive names, SHA-256 hashes, and coverage counts |
| `archive_entries.tsv.gz` | Entry path, size, compressed size, ZIP CRC-32 |
| `atlas_frames.tsv.gz` | TexturePacker frame names and trim/rect/offset/source-size metadata from readable `.plist` files |
| `unreadable_plists.tsv.gz` | Paths and parser error types for `.plist` files not indexed as atlas frames |

The OBB contains 41 `.plist` files that the standard plist parser could not read. Their paths are listed in `unreadable_plists.tsv.gz`; they are excluded from `atlas_frames.tsv.gz` and counted in the manifest. The archive entry list excludes directory records.

## Rebuild and validate

Run `ExportProgramIndex.java` as a Ghidra post-script against each program in a writable copy of the user's analyzed project. Its sole argument is the output path prefix. The script writes only metadata; it does not change the original Ghidra project. To regenerate archive metadata:

```bash
python3 tools/ghidra/index_archive_metadata.py \
  --apk /path/to/original.apk --obb /path/to/original.obb \
  --output docs/ghidra/full-index
```

Validate every compressed table and cross-check archive counts:

```bash
python3 tools/ghidra/verify_full_index.py docs/ghidra/full-index
```

The existing [focused SingleLogin index](../ghidra-login-index.tsv) remains useful for quick manual inspection. This bundle is the broader search base for future function, string, asset, and call-graph queries.
