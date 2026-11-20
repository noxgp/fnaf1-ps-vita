"""Inspect Clickteam Android data and prepare bounded-size Vita image fixtures.

This tool never executes code from the APK. It does not translate game events.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import io
import json
from pathlib import Path
import struct
import zipfile
import zlib

from PIL import Image, ImageDraw


def chunks(data: bytes, start: int = 0):
    pos = start
    while pos < len(data):
        if pos + 8 > len(data):
            raise ValueError(f"Truncated chunk header at {pos}")
        kind, flags, size = struct.unpack_from("<HHi", data, pos)
        end = pos + 8 + size
        if size < 0 or end > len(data):
            raise ValueError(f"Invalid chunk size at {pos}")
        payload = data[pos + 8:end]
        if flags == 1:
            expected, compressed = struct.unpack_from("<II", payload)
            payload = zlib.decompress(payload[8:8 + compressed])
            if len(payload) != expected:
                raise ValueError("Decompressed chunk size mismatch")
        elif flags != 0:
            raise ValueError(f"Unsupported chunk flags {flags}; refusing to guess")
        yield kind, pos, payload
        pos = end
        if kind == 0x7F7F:
            if pos != len(data):
                raise ValueError("Unexpected bytes after chunk terminator")
            return


def decode_image(mode: int, width: int, height: int, raw: bytes) -> Image.Image:
    if not (0 < width <= 16384 and 0 < height <= 16384):
        raise ValueError("Invalid image dimensions")
    if mode == 5:
        image = Image.open(io.BytesIO(raw)).convert("RGBA")
        if image.size != (width, height):
            raise ValueError("Encoded image dimensions mismatch")
        return image
    if mode == 0:
        if len(raw) != width * height * 4:
            raise ValueError("RGBA buffer size mismatch")
        return Image.frombytes("RGBA", (width, height), raw)
    if mode not in (1, 4):
        raise ValueError(f"Unsupported Android image mode {mode}")
    # Packed 16-bit rows are padded to an even number of pixels.
    stride = (width + width % 2) * 2
    if len(raw) != stride * height:
        raise ValueError(f"Packed buffer size mismatch: {len(raw)} != {stride * height}")
    result = bytearray(width * height * 4)
    for y in range(height):
        for x in range(width):
            value = struct.unpack_from("<H", raw, y * stride + x * 2)[0]
            if mode == 4:
                r, g, b, a = ((value >> 11) * 255 // 31,
                              ((value >> 5) & 63) * 255 // 63,
                              (value & 31) * 255 // 31, 255)
            else:
                r, g, b, a = ((value >> 12) * 17, ((value >> 8) & 15) * 17,
                              ((value >> 4) & 15) * 17, (value & 15) * 17)
            offset = (y * width + x) * 4
            result[offset:offset + 4] = bytes((r, g, b, a))
    return Image.frombytes("RGBA", (width, height), bytes(result))


def image_bank(data: bytes):
    maximum, count = struct.unpack_from("<HH", data)
    pos = 4
    seen = set()
    for _ in range(count):
        handle, mode, w, h, hx, hy, ax, ay, size = struct.unpack_from("<HIhhhhhhI", data, pos)
        pos += 22
        if size > len(data) - pos or handle in seen or handle >= maximum:
            raise ValueError("Invalid image bank entry")
        seen.add(handle)
        encoded = data[pos:pos + size]
        pos += size
        raw = encoded if mode == 5 and encoded.startswith(b"\xff\xd8") else zlib.decompress(encoded)
        image = decode_image(mode, w, h, raw)
        yield {"handle": handle, "mode": mode, "width": w, "height": h,
               "hotspot": [hx, hy], "action_point": [ax, ay]}, image
    if pos != len(data):
        raise ValueError("Image bank not consumed completely")


def extract(apk: Path, output: Path, previews=True):
    output.mkdir(parents=True, exist_ok=True)
    for folder in ("original", "images", "audio", "frames", "vita/images", "contacts"):
        (output / folder).mkdir(parents=True, exist_ok=True)
    with apk.open("rb") as file:
        sha256 = hashlib.file_digest(file, "sha256").hexdigest()
    report = {"apk_name": apk.name, "apk_sha256": sha256,
              "status": "asset extraction; game event runtime not implemented",
              "libraries": [], "frames": [], "images": [], "audio": [], "chunks": []}
    with zipfile.ZipFile(apk) as archive:
        data = archive.read("res/raw/application.ccn")
        if data[:4] != b"PAMU":
            raise ValueError("Expected Unicode Clickteam CCN")
        runtime, subversion, product, build = struct.unpack_from("<HHII", data, 4)
        report["clickteam"] = dict(runtime=runtime, subversion=subversion, product=product, build=build)
        (output / "original/application.ccn").write_bytes(data)
        report["libraries"] = [entry.filename for entry in archive.infolist() if entry.filename.startswith("lib/")]
        for entry in archive.infolist():
            name = Path(entry.filename).name
            if entry.filename.startswith("res/raw/s") and name.endswith((".ogg", ".wav", ".mp3")):
                audio = archive.read(entry)
                (output / "audio" / name).write_bytes(audio)
                report["audio"].append({"file": name, "bytes": len(audio)})
        for kind, offset, payload in chunks(data, 16):
            report["chunks"].append({"id": f"0x{kind:04x}", "offset": offset, "bytes": len(payload)})
            if kind in (0x2224, 0x2225, 0x223B):
                key = {0x2224: "title", 0x2225: "author", 0x223B: "copyright"}[kind]
                report[key] = payload.decode("utf-16le").rstrip("\0")
            if kind == 0x3333:
                frame = {"index": len(report["frames"]), "event_bytes": 0}
                for subkind, _, content in chunks(payload):
                    if subkind == 0x3335:
                        frame["name"] = content.decode("utf-16le").rstrip("\0")
                    elif subkind == 0x3334:
                        frame["width"], frame["height"] = struct.unpack_from("<II", content)
                    elif subkind == 0x333D:
                        frame["event_bytes"] = len(content)
                        (output / "frames" / f"{frame['index']:02d}.events.bin").write_bytes(content)
                (output / "frames" / f"{frame['index']:02d}.ccn-chunks.bin").write_bytes(payload)
                report["frames"].append(frame)
            if kind == 0x6666:
                for metadata, image in image_bank(payload):
                    handle = metadata["handle"]
                    image.save(output / "images" / f"{handle:04d}.png")
                    if previews:
                        # Original PNG retains full resolution. Only fixtures for the viewer shrink.
                        image.thumbnail((960, 480), Image.Resampling.LANCZOS)
                        w, h = image.size
                        (output / "vita/images" / f"{handle:04d}.rgba").write_bytes(
                            struct.pack("<4sHH", b"FIMG", w, h) + image.tobytes())
                        metadata["preview_width"], metadata["preview_height"] = w, h
                    report["images"].append(metadata)
                    if len(report["images"]) % 50 == 0:
                        print(f"Decoded {len(report['images'])} images", flush=True)
    report["images"].sort(key=lambda item: item["handle"])
    report["image_modes"] = dict(Counter(item["mode"] for item in report["images"]))
    (output / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    # Small text index lets the native proof build avoid any JSON or image decoding dependency.
    (output / "vita/images.txt").write_text("".join(f"{item['handle']}\n" for item in report["images"]), encoding="ascii")
    if previews:
        for page in range((len(report["images"]) + 79) // 80):
            sheet = Image.new("RGB", (1280, 1000), "#20252c")
            draw = ImageDraw.Draw(sheet)
            for cell, item in enumerate(report["images"][page * 80:(page + 1) * 80]):
                image = Image.open(output / "images" / f"{item['handle']:04d}.png").convert("RGBA")
                image.thumbnail((154, 100))
                x, y = cell % 8 * 160, cell // 8 * 100
                # Leave a label and enough separation for identification.
                image.thumbnail((154, 78))
                sheet.paste(image, (x + (160 - image.width) // 2, y), image)
                draw.text((x + 3, y + 80), f"{item['handle']}  {item['width']}x{item['height']}", fill="white")
            sheet.save(output / "contacts" / f"page-{page + 1}.jpg", quality=88)
    print(f"Extracted {len(report['images'])} images, {len(report['audio'])} sounds, {len(report['frames'])} frames")
    print(f"Report: {output / 'report.json'}")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk", type=Path)
    parser.add_argument("--output", type=Path, default=Path("data"))
    args = parser.parse_args()
    extract(args.apk, args.output)
