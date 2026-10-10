# EnemyActions action-data validation

The repository contains a structural clean-room parser for the currently recovered `EnemyActionsData::loadWBGFile` Sections A-G. Synthetic tests prove bounded behavior, and three genuine user-owned `.actData` files from the original APK now validate through the complete recovered A-G boundary to exact EOF.

`tools/enemy_actions_wbg_validate.cpp` is the host-side bridge for this work. The historical `wbg` name follows the original method name; native evidence shows that `loadWBGFile` does not impose a `.wbg` suffix. The caller supplies the filename, and original APK resources use `.actData`.

The validator does not bundle, upload, or modify game assets. It reads a local action-data file, runs the project-owned parser, and prints only a structural summary.

## Build

From the repository root:

```sh
g++ -std=c++17 -Wall -Wextra -Werror \
  -Iandroid/app/src/main/cpp \
  android/app/src/main/cpp/hp_data_reader.cpp \
  android/app/src/main/cpp/enemy_actions_layout_evidence.cpp \
  android/app/src/main/cpp/enemy_actions_wbg_prefix.cpp \
  android/app/src/main/cpp/enemy_actions_wbg_combo_section.cpp \
  android/app/src/main/cpp/enemy_actions_wbg_final_table.cpp \
  android/app/src/main/cpp/enemy_actions_wbg_document.cpp \
  tools/enemy_actions_wbg_validate.cpp \
  -o /tmp/enemy_actions_wbg_validate
```

## Run against a user-owned file

```sh
/tmp/enemy_actions_wbg_validate /path/to/action.actData
```

The validator is suffix-agnostic; the batch scanner selects known action-data suffixes. Successful output is line-oriented `key=value` data suitable for diffing or attaching to evidence notes. It reports:

- input size, recovered bytes consumed, and trailing bytes;
- header word and primary-record count;
- Section B compact-record count;
- Section C nested group/record totals;
- Section D selected group count and record total;
- Section E fixed-tail record count;
- Section F tuple count plus derived `+0x94/+0x98/+0x9c` object counts;
- Section G entry count.

The tool deliberately does not print parsed strings, proprietary payload bytes, or complete record contents.

## Batch validation of an imported asset tree

`tools/validate_enemy_actions_wbg_tree.py` recursively finds `.actData` and `.wbg` files case-insensitively, invokes the compiled validator for each file, and prints one compact JSON object per file. `.actData` is now the evidence-backed original suffix; `.wbg` remains accepted for research fixtures and any user-owned files that use that naming.

The Android importer activates user-owned resources beneath the app-private `filesDir/assets` tree. For a host-side extracted/imported copy of that asset tree, run:

```sh
python tools/validate_enemy_actions_wbg_tree.py \
  --validator /tmp/enemy_actions_wbg_validate \
  /path/to/imported/assets \
  > /tmp/nevergone-action-validation.jsonl
```

The scanner sorts relative paths for deterministic reports and recognizes mixed-case suffixes such as `action.actData`, `ACTION.ACTDATA`, `action.wbg`, and `ACTION.WBG`.

Batch exit codes:

- `0`: at least one supported action-data file was found and every validator invocation succeeded;
- `1`: invalid root/validator arguments;
- `4`: no supported action-data files were found beneath the supplied root;
- `5`: one or more files failed the selected validator policy.

For the experimental strict-EOF policy across the whole tree:

```sh
python tools/validate_enemy_actions_wbg_tree.py \
  --validator /tmp/enemy_actions_wbg_validate \
  --require-eof \
  /path/to/imported/assets
```

A failed file remains in the JSONL report with its `validator_exit` and any structural metadata emitted before the validator rejected the selected policy. The batch tool does not copy, decode, upload, or print action-data payload contents.

## EOF policy

By default, trailing bytes are reported rather than rejected because the original parser's malformed/trailing-file policy has not been proven:

```text
eof_policy=reported
```

For a local experiment that requires the recovered Sections A-G to consume the whole file, use:

```sh
/tmp/enemy_actions_wbg_validate --require-eof /path/to/action.actData
```

That mode exits with code `3` when `trailing_bytes != 0`. This remains a validation option rather than a general claim about original malformed-file behavior. The three genuine `.actData` samples recorded in `docs/evidence/enemy-actions-actdata-validation.md` all happen to reach exact EOF.

Single-file exit codes:

- `0`: parsed successfully and the selected EOF policy passed;
- `1`: usage or file-read failure;
- `2`: recovered A-G parser rejected the file;
- `3`: parse succeeded but `--require-eof` found trailing bytes.

## Evidence boundary

A successful run proves only that the recovered stream topology accepts that particular user-owned file and yields internally bounded section counts. It does not prove gameplay semantics, `ActionFrameData` field meanings, enemy spawning, combat behavior, or universal EOF behavior.

See `docs/evidence/enemy-actions-actdata-validation.md` for the first genuine APK-backed validation set and the native filename/path evidence.
