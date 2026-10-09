# Nevergone OBB inspector / unpacker

`tools/obb_unpack.py` is a streaming, standard-library-first helper for large Android expansion files. A 600+ MiB OBB does not need to be uploaded to the repository or chat.

## 1. Inspect first

```bash
python3 tools/obb_unpack.py inspect /path/to/main.*.com.hippiegame.nevergone.obb -o obb_diagnostic
```

This creates:

- `obb_diagnostic/report.json` — container type, SHA-256, size, entropy samples, Android JOBB footer, archive summary, embedded format signatures and offsets of Never Gone-relevant strings;
- `obb_diagnostic/interesting_strings.txt` — compact readable contexts around `ServerList`, `btn_standard`, `gamescene_ui`, `.plist`, `.png`, `.lua`, etc.;
- `obb_diagnostic/head.bin` — first at most 1 MiB;
- `obb_diagnostic/tail.bin` — last at most 1 MiB.

Share `report.json` first. If a custom container still cannot be identified, the two 1 MiB samples are normally enough for the next reverse-engineering pass; the full OBB is not required.

## 2. Try extraction

```bash
python3 tools/obb_unpack.py unpack /path/to/main.*.obb -o obb_extracted
```

Recognized paths:

- normal ZIP expansion files: extracted directly with Python and path-traversal protection;
- TAR-family archives: extracted directly;
- Android JOBB files: the standard footer is parsed, including package/version/overlay/encrypted flags;
- unencrypted JOBB FAT images: `jobb`, `mcopy` or `7z` is used when available;
- other containers readable by `7z`: delegated locally to `7z`/`7zz`.

On Manjaro the useful optional helpers are:

```bash
sudo pacman -S p7zip mtools
```

The Python ZIP/TAR/inspection path has no third-party Python dependency.

## Encrypted JOBB

If the Android footer contains the `OBB_SALTED` flag, the tool reports an encrypted JOBB instead of guessing a key. If the original password is known and the legacy Android `jobb` utility is installed:

```bash
python3 tools/obb_unpack.py unpack game.obb -o obb_extracted --password 'PASSWORD'
```

No password is written to reports or repository files.

## Files useful to the recompilation

After successful extraction, the current priorities are:

```text
Common/btn_standard_a.png
Common/btn_standard_b.png
Common/btn_standard_c.png
gamescene_ui/ServerList/
gamescene_ui/ServerList/XMLFile1.xml
```

Also preserve any `.plist`, `.png`, `.lua`, `.csv`, `.xml`, `.bff`, audio and TexturePacker atlas files. Do not commit proprietary original resources to the public repository; use them only as local/user-imported clean-room inputs.
