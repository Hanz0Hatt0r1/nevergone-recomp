#!/usr/bin/env python3
from __future__ import annotations

import csv
import tempfile
import unittest
from pathlib import Path

from adapt_revenginstr_parameter_store_field import (
    FIELDS,
    GENERIC_FORMAT,
    NEVERGONE_FORMAT,
    adapt_file,
)

ROW = {
    "format": GENERIC_FORMAT,
    "function_entry": "0032c75a",
    "function_name": "MEPlayer::setPlayerDir(int)",
    "store_address": "0032c77c",
    "store_time": "19",
    "destination_parameter_index": "0",
    "destination_offset": "0x48",
    "destination_pattern": "int_add",
    "source_parameter_index": "1",
    "value_size": "4",
    "status": "parameter_to_parameter_field",
}


class ParameterStoreAdapterTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="parameter-store-adapter-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def write(self, row: dict[str, str]) -> Path:
        path = self.root / "generic.tsv"
        with path.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=FIELDS, delimiter="\t", lineterminator="\n")
            writer.writeheader()
            writer.writerow(row)
        return path

    def test_adapts_only_the_format_cell(self) -> None:
        source = self.write(ROW)
        output = self.root / "nested" / "legacy.tsv"
        adapt_file(source, output)
        with output.open(encoding="utf-8", newline="") as handle:
            reader = csv.DictReader(handle, delimiter="\t")
            self.assertEqual(reader.fieldnames, list(FIELDS))
            rows = list(reader)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["format"], NEVERGONE_FORMAT)
        self.assertEqual({key: value for key, value in rows[0].items() if key != "format"}, {
            key: value for key, value in ROW.items() if key != "format"
        })

    def test_rejects_foreign_format_without_partial_output(self) -> None:
        row = dict(ROW, format="consumer.ParameterStoreField/1")
        source = self.write(row)
        output = self.root / "legacy.tsv"
        with self.assertRaisesRegex(ValueError, GENERIC_FORMAT):
            adapt_file(source, output)
        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
