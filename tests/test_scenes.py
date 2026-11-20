import struct
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from decode_scenes import common_properties, wide_string, animation_bank


class SceneFormatTests(unittest.TestCase):
    def test_build_296_header_offsets_and_visibility(self):
        # This order differs from the older Android layout used by CTFAK.
        data = bytearray(116)
        struct.pack_into("<I", data, 0, len(data))
        struct.pack_into("<H", data, 4, 70)  # Movement data.
        struct.pack_into("<I", data, 16, 0x00060334)
        struct.pack_into("<8h", data, 20, *([-1] * 8))
        struct.pack_into("<HH", data, 42, 8, 1275)
        data[46:50] = b"SPRI"
        result = common_properties(bytes(data), 2)
        self.assertEqual(result["identifier"], "SPRI")
        self.assertTrue(result["visible_at_start"])
        self.assertFalse(result["create_at_start"])
        self.assertEqual(result["offsets"]["movement"], 70)

    def test_offset_outside_object_rejected(self):
        data = bytearray(70)
        struct.pack_into("<I", data, 0, len(data))
        struct.pack_into("<H", data, 36, 200)
        with self.assertRaises(ValueError):
            common_properties(bytes(data), 2)

    def test_animation_keeps_sparse_ids_repeat_and_hot_frame_order(self):
        data = bytearray(90)
        struct.pack_into("<HHHH", data, 0, 90, 2, 0, 8)
        struct.pack_into("<H", data, 8 + 16 * 2, 64)
        struct.pack_into("<BBHHH3H", data, 72, 10, 20, 0, 1, 3, 9, 4, 9)
        animations = animation_bank(bytes(data), 0)
        self.assertEqual(len(animations), 1)
        self.assertEqual(animations[0]["animation"], 1)
        direction = animations[0]["directions"][0]
        self.assertEqual(direction["direction"], 16)
        self.assertEqual(direction["images"], [9, 4, 9])
        self.assertEqual(direction["back_to"], 1)

    def test_utf16_terminator_is_code_unit_aligned(self):
        data = "ĀA".encode("utf-16le") + b"\0\0"
        self.assertEqual(wide_string(data), ("ĀA", 6))
        with self.assertRaises(ValueError):
            wide_string(data[:-1])
