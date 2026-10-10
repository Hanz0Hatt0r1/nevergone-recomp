# EnemyActions WBG validation

The repository contains a structural clean-room parser for the currently recovered `EnemyActionsData::loadWBGFile` Sections A-G. Synthetic tests prove the parser's bounded behavior, but genuine user-owned WBG files still need validation.

`tools/enemy_actions_wbg_validate.cpp` is the host-side bridge for that step. It does not bundle, upload, or modify game assets. It reads a local WBG file, runs the project-owned parser, and prints only a structural summary.

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
/tmp/enemy_actions_wbg_validate /path/to/file.wbg
```

Successful output is line-oriented `key=value` data suitable for diffing or attaching to evidence notes. It reports:

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

`tools/validate_enemy_actions_wbg_tree.py` recursively finds files whose suffix is `.wbg` case-insensitively, invokes the compiled validator for each file, and prints one compact JSON object per file. Only the relative path, validator exit code, and structural counters are emitted.

The Android importer activates user-owned resources beneath the app-private `filesDir/assets` tree. For a host-side extracted/imported copy of that asset tree, run:

```sh
python tools/validate_enemy_actions_wbg_tree.py \
  --validator /tmp/enemy_actions_wbg_validate \
  /path/to/imported/assets \
  > /tmp/nevergone-wbg-validation.jsonl
```

The scanner sorts relative paths for deterministic reports and recognizes names such as both `action.wbg` and `ACTION.WBG`.

Batch exit codes:

- `0`: at least one WBG file was found and every validator invocation succeeded;
- `1`: invalid root/validator arguments;
- `4`: no WBG files were found beneath the supplied root;
- `5`: one or more WBG files failed the selected validator policy.

For the experimental strict-EOF policy across the whole tree:

```sh
python tools/validate_enemy_actions_wbg_tree.py \
  --validator /tmp/enemy_actions_wbg_validate \
  --require-eof \
  /path/to/imported/assets
```

A failed file remains in the JSONL report with its `validator_exit` and any structural metadata emitted before the validator rejected the selected policy. The batch tool does not copy, decode, upload, or print WBG payload contents.

## EOF policy

By default, trailing bytes are reported rather than rejected because the original parser's EOF policy has not been proven:

```text
eof_policy=reported
```

For a local experiment that requires the recovered Sections A-G to consume the whole file, use:

```sh
/tmp/enemy_actions_wbg_validate --require-eof /path/to/file.wbg
```

That mode exits with code `3` when `trailing_bytes != 0`. This is a validation option only; it is not a claim about the original game's malformed-file behavior.

Single-file exit codes:

- `0`: parsed successfully and the selected EOF policy passed;
- `1`: usage or file-read failure;
- `2`: recovered A-G parser rejected the file;
- `3`: parse succeeded but `--require-eof` found trailing bytes.

## Evidence boundary

A successful run proves only that the recovered stream topology accepts that particular user-owned file and yields internally bounded section counts. It does not prove gameplay semantics, ActionFrameData field meanings, enemy spawning, or combat behavior.
