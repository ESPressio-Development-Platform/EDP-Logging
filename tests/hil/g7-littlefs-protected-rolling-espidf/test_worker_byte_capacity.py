import unittest
from audit_worker_byte_capacity import verify_capacity

class ByteCapacityEvidenceTests(unittest.TestCase):
    def test_exact_limit(self):
        self.assertEqual(verify_capacity("EDP_BYTE_CAPACITY|PASS|limit=277|peak=277\n", 277, 198), 277)

    def test_rejects_unqualified_witnesses(self):
        cases = [
            ("", 277, 198),
            ("EDP_BYTE_CAPACITY|PASS|limit=278|peak=277", 277, 198),
            ("EDP_BYTE_CAPACITY|PASS|limit=277|peak=278", 277, 198),
            ("EDP_BYTE_CAPACITY|PASS|limit=277|peak=197", 277, 198),
            ("EDP_BYTE_CAPACITY|PASS|limit=277|peak=277", 277, 40),
            ("EDP_BYTE_CAPACITY|PASS|limit=277|peak=277", 0, 198),
            ("EDP_BYTE_CAPACITY|PASS|limit=277|peak=277 extra", 277, 198),
            ("EDP_BYTE_CAPACITY|PASS|limit=277|peak=277\nEDP_EDLS|HALT|failure", 277, 198),
            ("EDP_BYTE_CAPACITY|PASS|limit=277|peak=277\n" * 2, 277, 198),
        ]
        for text, limit, final in cases:
            with self.subTest(text=text, limit=limit, final=final):
                with self.assertRaises(ValueError):
                    verify_capacity(text, limit, final)

if __name__ == "__main__":
    unittest.main()
