import unittest
from relr_plan import decode_relr

class RelrTests(unittest.TestCase):
    def test_direct_and_bitmap(self):
        self.assertEqual(decode_relr([0x100, 1 | 2 | (1 << 31), 3]),
                         [0x100, 0x104, 0x17c, 0x180])
    def test_empty_bitmap_advances_cursor(self):
        self.assertEqual(decode_relr([0x100, 1, 3]), [0x100, 0x180])
    def test_new_address_resets_cursor(self):
        self.assertEqual(decode_relr([0x100, 3, 0x200, 3]), [0x100,0x104,0x200,0x204])
    def test_invalid_sequences(self):
        for words in ([3], [0x102], [0x100,0x100], [0x100,3,0x104]):
            with self.subTest(words=words), self.assertRaises(ValueError):
                decode_relr(words)

if __name__ == '__main__': unittest.main()
