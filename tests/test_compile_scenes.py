from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from compile_scenes import NumericParser, Unsupported


def token(typ, opcode, data=b""):
    return struct.pack("<hhH", typ, opcode, 6 + len(data)) + data


class NumericExpressionTests(unittest.TestCase):
    def test_android_global_index_is_a_16_bit_field(self):
        data = b"\0\0" + token(-1, 24, struct.pack("<IH", 0, 10)) + b"\0" * 4
        self.assertEqual(NumericParser(data).parse(), [("EX_VAR", -1, 10, 0)])

    def test_multiplication_precedes_addition(self):
        data = b"\0\0" + token(-1,0,struct.pack("<i",2)) + token(0,2)
        data += token(-1,0,struct.pack("<i",3)) + token(0,6) + token(-1,0,struct.pack("<i",4)) + b"\0" * 4
        self.assertEqual([item[0] for item in NumericParser(data).parse()], ["EX_CONST","EX_CONST","EX_CONST","EX_MUL","EX_ADD"])

    def test_fusion_min_max_argument_boundaries(self):
        data = b"\0\0" + token(-1,40) + token(-1,0,struct.pack("<i",4)) + token(-1,-3)
        data += token(-1,41) + token(-1,0,struct.pack("<i",1)) + token(-1,-3)
        data += token(-1,0,struct.pack("<i",2)) + token(-1,-2) + token(-1,-2) + b"\0" * 4
        self.assertEqual([item[0] for item in NumericParser(data).parse()], ["EX_CONST","EX_CONST","EX_CONST","EX_MAX","EX_MIN"])

    def test_unimplemented_expression_is_rejected(self):
        data = b"\0\0" + token(-1,999) + b"\0" * 4
        with self.assertRaises(Unsupported):
            NumericParser(data).parse()

    def test_trailing_token_bytes_are_rejected(self):
        with self.assertRaises(Unsupported):
            NumericParser(b"\0\0" + b"\0" * 4 + b"extra").parse()
