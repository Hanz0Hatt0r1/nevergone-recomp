# CharacterNameLayer random-name contract

This note records clean-room behavior recovered from the shipped ARMv7 `CharacterNameLayer::menuCloseCallback(CCObject*)` tag-3 path. Original `RandomName.csv` contents and proprietary binary/disassembly dumps are not stored in the repository.

## Source data

The shipped callback opens runtime resource:

```text
RandomName.csv
```

The imported APK copy is decoded by the existing user-owned asset importer and is expected at:

```text
<filesDir>/assets/RandomName.csv
```

The recovered native path treats the table as rows of three CSV columns. Random-name composition uses column `0` from one random row and column `1` from a second independently selected random row. The third column is not used by this callback.

## Recovered random row selection

The shipped callback calls `lrand48()` independently for each name component. The ARMv7 floating-point sequence corresponds to:

```text
unit   = float(lrand48()) * 2^-31
scaled = abs(unit - 0.01f) * rowCount
index  = truncate_to_integer(scaled)
```

`character_random_name::recovered_row_index()` reproduces this using single-precision arithmetic. The runtime also clamps a defensive out-of-range result to the last row; valid `lrand48()` output and a non-empty table normally keep the reconstructed index in range.

## Name composition

For independently selected rows `A` and `B`, the shipped callback reads:

```text
first  = table[A][0]
second = table[B][1]
```

and formats:

```text
%s%s
```

The resulting UTF-8 byte buffer is measured with `strlen`. If it is longer than the CharacterNameLayer limit of `18` bytes, the callback discards the second component and copies only `first` into the edit-box buffer.

This is distinct from the separate legacy ChooseHero submit limit of 21 bytes.

## CharacterNameLayer integration

`CharacterNameLayer::CretaUI(career)` immediately dispatches its tag-3 random-name control after creating the edit box. The recompilation therefore begins `character_name_state` with a pending randomize request.

`character_random_name::fulfill_pending()`:

1. verifies a CharacterNameLayer randomize request is still pending;
2. loads the user-imported `RandomName.csv`;
3. performs two recovered random row selections;
4. generates the combined/fallback name;
5. verifies the CharacterNameLayer generation and career did not change;
6. consumes the pending randomize request;
7. writes the generated text back into `character_name_state`.

The deterministic `fulfill_pending_with_random_values()` entry point exists for host smoke tests and uses the same generation path.

## Current boundary

This increment reconstructs the data/parser/generator side only. It does not embed the original CSV, seed or globally replace libc's `lrand48()` state, draw the CharacterNameLayer, or create an Android text field. Those presentation/input pieces remain separate from the recovered random-name semantics.
