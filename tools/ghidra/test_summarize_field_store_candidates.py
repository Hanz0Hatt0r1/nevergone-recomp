#!/usr/bin/env python3

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from summarize_field_store_candidates import read_candidates, render_summary

HEADER = (
    "function_entry\tfunction_name\tstore_address\tstore_time\tfield_offset_hex\t"
    "address_opcode\taddress_op_address\taddress_op_time\tbase_token\tvalue_token\t"
    "value_is_constant\tvalue_constant_hex\n"
)


class FieldStoreCandidateSummaryTests(unittest.TestCase):
    def write(self, body: str) -> Path:
        temp = tempfile.NamedTemporaryFile("w", encoding="utf-8", delete=False)
        temp.write(HEADER)
        temp.write(body)
        temp.close()
        self.addCleanup(lambda: Path(temp.name).unlink(missing_ok=True))
        return Path(temp.name)

    def test_accepts_constant_and_derived_stores(self) -> None:
        path = self.write(
            "00300000\tEnemyActionsSystem::init\t00300020\t1\t0x294\tPTRSUB\t"
            "0030001c\t0\tregister:r0:4\tconst:0x0:4\t1\t0x0\n"
            "00310000\tEnemyActionsSystem::showActionLastFrame\t00310014\t2\t0x294\t"
            "INT_ADD\t00310010\t0\tregister:r0:4\tdef:00310008:0:4\t0\t-\n"
        )
        rows = read_candidates(path, 0x294)
        self.assertEqual(len(rows), 2)
        self.assertTrue(rows[0].value_is_constant)
        self.assertEqual(rows[0].value_constant, 0)
        self.assertFalse(rows[1].value_is_constant)
        self.assertIsNone(rows[1].value_constant)
        report = render_summary(rows)
        self.assertIn("candidate_stores=2", report)
        self.assertIn("constant_stores=1", report)
        self.assertIn("constant_zero_stores=1", report)
        self.assertIn("value=derived", report)

    def test_rejects_wrong_field_offset(self) -> None:
        path = self.write(
            "00300000\tEnemyActionsSystem::init\t00300020\t1\t0x290\tPTRSUB\t"
            "0030001c\t0\tregister:r0:4\tconst:0x0:4\t1\t0x0\n"
        )
        with self.assertRaisesRegex(ValueError, "unexpected field offset"):
            read_candidates(path, 0x294)

    def test_rejects_nonconstant_with_constant_payload(self) -> None:
        path = self.write(
            "00300000\tEnemyActionsSystem::init\t00300020\t1\t0x294\tPTRSUB\t"
            "0030001c\t0\tregister:r0:4\tdef:00300008:0:4\t0\t0x0\n"
        )
        with self.assertRaisesRegex(ValueError, "non-constant row"):
            read_candidates(path, 0x294)

    def test_rejects_duplicate_store_identity(self) -> None:
        row = (
            "00300000\tEnemyActionsSystem::init\t00300020\t1\t0x294\tPTRSUB\t"
            "0030001c\t0\tregister:r0:4\tconst:0x0:4\t1\t0x0\n"
        )
        path = self.write(row + row)
        with self.assertRaisesRegex(ValueError, "duplicate store candidate"):
            read_candidates(path, 0x294)

    def test_rejects_unexpected_address_opcode(self) -> None:
        path = self.write(
            "00300000\tEnemyActionsSystem::init\t00300020\t1\t0x294\tPTRADD\t"
            "0030001c\t0\tregister:r0:4\tconst:0x0:4\t1\t0x0\n"
        )
        with self.assertRaisesRegex(ValueError, "unexpected address opcode"):
            read_candidates(path, 0x294)


if __name__ == "__main__":
    unittest.main()
