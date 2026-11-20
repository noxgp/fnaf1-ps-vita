import io
from pathlib import Path
import struct
import sys
import unittest

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from extract_apk import chunks, decode_image
from audit_events import parse_events, parse_record


class ImageFormatTests(unittest.TestCase):
    def test_rgba_keeps_transparent_color_and_alpha(self):
        source = bytes((255, 20, 0, 0, 10, 0, 255, 128))
        self.assertEqual(decode_image(0, 2, 1, source).tobytes(), source)

    def test_rgb565_primaries_and_odd_row_padding(self):
        source = struct.pack("<HHHH", 0xF800, 0x07E0, 0x001F, 0xFFFF)
        image = decode_image(4, 3, 1, source)
        self.assertEqual([image.getpixel((x, 0)) for x in range(image.width)], [(255, 0, 0, 255), (0, 255, 0, 255), (0, 0, 255, 255)])

    def test_rgba4444_has_full_white_and_half_alpha(self):
        image = decode_image(1, 2, 1, struct.pack("<HH", 0xFFFF, 0xF008))
        self.assertEqual([image.getpixel((x, 0)) for x in range(image.width)], [(255, 255, 255, 255), (255, 0, 0, 136)])

    def test_encoded_image_dimensions_checked(self):
        encoded = io.BytesIO()
        Image.new("RGB", (2, 3), (40, 70, 90)).save(encoded, format="JPEG")
        self.assertEqual(decode_image(5, 2, 3, encoded.getvalue()).size, (2, 3))
        with self.assertRaises(ValueError):
            decode_image(5, 3, 2, encoded.getvalue())

    def test_invalid_dimensions_and_truncated_buffers_rejected(self):
        for mode, width, height, data in [(0, 0, 2, b""), (0, 2, 2, b"\0" * 15), (4, 1, 1, b"\0\0")]:
            with self.subTest(mode=mode, width=width), self.assertRaises(ValueError):
                decode_image(mode, width, height, data)


class EventFormatTests(unittest.TestCase):
    def test_action_preserves_parameter_payload(self):
        parameter = struct.pack("<HHI", 8, 23, 0xAABBCCDD)
        data = struct.pack("<HhhHhBBBB", 22, -1, 3, 4, -1, 0, 0, 1, 0) + parameter
        action, end = parse_record(data, 0, False)
        self.assertEqual(end, len(data))
        self.assertEqual(action["opcode"], 3)
        self.assertEqual(action["parameters"], [{"code": 23, "payload_hex": "ddccbbaa"}])

    def test_empty_event_file_and_trailing_junk(self):
        data = b"ER>>" + struct.pack("<20hH", *([0] * 21)) + b"<<ER"
        self.assertEqual(parse_events(data)["groups"], [])
        with self.assertRaises(ValueError):
            parse_events(data + b"junk")

    def test_bad_record_size_rejected(self):
        with self.assertRaises(ValueError):
            parse_record(b"\0" * 16, 0, True)

    def test_chunk_outside_file_rejected(self):
        with self.assertRaises(ValueError):
            list(chunks(struct.pack("<HHi", 0x6666, 0, 100) + b"short"))


if __name__ == "__main__":
    unittest.main()
