import json
from pathlib import Path
import tempfile
import unittest

from save_probe_plan import plan
from verify_save_probes import verify


EVIDENCE = Path(__file__).resolve().parents[2] / 'docs/evidence/unidbg-2026-10-10/save-probes.jsonl'


class SaveProbeTests(unittest.TestCase):
    def test_native_evidence_matches_reconstruction(self):
        verify(EVIDENCE)

    def test_plan_covers_both_directions_and_counter_wrap(self):
        specs = plan()
        self.assertEqual(len(specs), 36)
        self.assertEqual({s['symbol'].split('6')[-1][:6] for s in specs}, {'Encode', 'Decode'})
        self.assertEqual({len(bytes.fromhex(s['input_hex'])) for s in specs}, {0, 1, 126, 127, 128, 255})

    def test_corrupted_native_output_is_rejected(self):
        rows = [json.loads(line) for line in EVIDENCE.read_text().splitlines()]
        rows[1]['memory_after']['guard'] = 0
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'changed.jsonl'
            path.write_text('\n'.join(json.dumps(row) for row in rows) + '\n')
            with self.assertRaises(AssertionError):
                verify(path)


if __name__ == '__main__':
    unittest.main()
