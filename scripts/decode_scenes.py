"""Decode original Fusion build 296 Android objects, animations and scene layout.

All object/image references are checked before writing the intermediate format.
Unknown extension data and instance trailers are retained, not interpreted.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import struct

from extract_apk import chunks


def read_u16(data, offset):
    return struct.unpack_from("<H", data, offset)[0]


def wide_string(data, offset=0):
    end = offset
    while end + 2 <= len(data) and data[end:end + 2] != b"\0\0":
        end += 2
    if end + 2 > len(data):
        raise ValueError("Unterminated UTF-16 string")
    return data[offset:end].decode("utf-16le"), end + 2


def animation_bank(data, base):
    size, count = struct.unpack_from("<HH", data, base)
    if count > 256 or base + 4 + count * 2 > len(data):
        raise ValueError("Invalid animation bank")
    result = []
    for animation in range(count):
        offset = read_u16(data, base + 4 + animation * 2)
        if not offset:
            continue
        position = base + offset
        if position + 64 > len(data):
            raise ValueError("Animation direction table outside object")
        directions = []
        for direction in range(32):
            relative = read_u16(data, position + direction * 2)
            if not relative:
                continue
            start = position + relative
            minimum, maximum, repeat, back_to, frames = struct.unpack_from("<BBHHH", data, start)
            if start + 8 + frames * 2 > len(data):
                raise ValueError("Animation frames outside object")
            handles = list(struct.unpack_from(f"<{frames}H", data, start + 8))
            directions.append(dict(direction=direction, minimum_speed=minimum, maximum_speed=maximum,
                                   repeat=repeat, back_to=back_to, images=handles))
        result.append(dict(animation=animation, directions=directions))
    return result


def text_properties(data, base):
    size, width, height, count = struct.unpack_from("<IIII", data, base)
    if size > len(data) - base or count > 4096:
        raise ValueError("Invalid text bank")
    paragraphs = []
    for paragraph in range(count):
        position = base + struct.unpack_from("<I", data, base + 16 + paragraph * 4)[0]
        font, flags = struct.unpack_from("<HH", data, position)
        color = list(data[position + 4:position + 8])
        value, _ = wide_string(data, position + 8)
        paragraphs.append(dict(font=font, flags=flags, color=color, value=value))
    return dict(width=width, height=height, paragraphs=paragraphs)


def counter_properties(data, base):
    size, width, height, player, display_type, flags, font = struct.unpack_from("<IIIHHHH", data, base)
    result = dict(width=width, height=height, display_type=display_type, flags=flags, font=font)
    if display_type in (1, 4, 50):
        count = read_u16(data, base + 20)
        result["images"] = list(struct.unpack_from(f"<{count}H", data, base + 22))
    elif display_type not in (0, 2, 3, 5):
        raise ValueError(f"Unknown counter display type {display_type}")
    if base + size > len(data):
        raise ValueError("Counter display outside object")
    return result


def common_properties(data, object_type):
    if len(data) < 70 or struct.unpack_from("<I", data)[0] != len(data):
        raise ValueError("Invalid build 296 object header")
    # Layout verified against the actual build 296 data; older CTFAK Android
    # headers use a different order, so do not substitute their field offsets.
    movement, values, strings, counter, system, extension = struct.unpack_from("<6H", data, 4)
    flags = struct.unpack_from("<I", data, 16)[0]
    animation = read_u16(data, 36)
    new_flags, preferences = struct.unpack_from("<HH", data, 42)
    identifier = data[46:50].decode("latin1")
    offsets = dict(movement=movement, values=values, strings=strings, counter=counter,
                   system=system, extension=extension, animation=animation)
    if any(offset and (offset < 70 or offset >= len(data)) for offset in offsets.values()):
        raise ValueError(f"Object offset outside payload: {offsets}")
    result = dict(identifier=identifier, flags=flags, new_flags=new_flags, preferences=preferences,
                  visible_at_start=bool(new_flags & 8), create_at_start=not bool(flags & 0x20000),
                  qualifiers=list(struct.unpack_from("<8h", data, 20)), offsets=offsets,
                  # Preserve header words whose meaning has not been verified.
                  header_hex=data[:70].hex())
    if animation:
        result["animations"] = animation_bank(data, animation)
    if values:
        count = read_u16(data, values)
        if values + 2 + count * 4 > len(data):
            raise ValueError("Alterable values outside object")
        result["alterable_values"] = list(struct.unpack_from(f"<{count}i", data, values + 2))
        tail = values + 2 + count * 4
        if tail + 4 <= len(data):
            result["alterable_flags"] = struct.unpack_from("<I", data, tail)[0]
    if strings:
        count = read_u16(data, strings)
        position = strings + 2
        result["alterable_strings"] = []
        for _ in range(count):
            value, position = wide_string(data, position)
            result["alterable_strings"].append(value)
    if counter:
        size, initial, minimum, maximum = struct.unpack_from("<Hiii", data, counter)
        result["counter"] = dict(initial=initial, minimum=minimum, maximum=maximum)
    if system and object_type == 3:
        result["text"] = text_properties(data, system)
    elif system and object_type == 7:
        result["counter_display"] = counter_properties(data, system)
    if extension:
        size = struct.unpack_from("<I", data, extension)[0]
        if size < 20 or extension + size > len(data):
            raise ValueError("Extension outside object")
        result["extension_hex"] = data[extension:extension + size].hex()
    return result


def object_bank(data):
    count = struct.unpack_from("<I", data)[0]
    position = 4
    result = []
    for _ in range(count):
        obj = {}
        while True:
            kind, flags, size = struct.unpack_from("<HHi", data, position)
            end = position + 8 + size
            if flags or size < 0 or end > len(data):
                raise ValueError("Invalid object chunk")
            payload = data[position + 8:end]
            position = end
            if kind == 0x4444:
                handle, object_type, flags = struct.unpack_from("<HhH", payload)
                obj.update(handle=handle, type=object_type, flags=flags, header_hex=payload.hex(),
                           ink_effect=struct.unpack_from("<I", payload, 8)[0], coefficient=list(payload[12:16]))
            elif kind == 0x4445:
                obj["name"], _ = wide_string(payload)
            elif kind == 0x4446:
                if obj["type"] == 1:
                    size, obstacle, collision, width, height, image = struct.unpack_from("<IHHIIH", payload)
                    obj["backdrop"] = dict(width=width, height=height, image=image)
                elif obj["type"] >= 2:
                    obj["common"] = common_properties(payload, obj["type"])
                else:
                    obj["properties_hex"] = payload.hex()
            elif kind == 0x7F7F:
                break
            else:
                obj.setdefault("extra_chunks", []).append(dict(id=kind, payload_hex=payload.hex()))
        result.append(obj)
    if position != len(data) or len({obj["handle"] for obj in result}) != count:
        raise ValueError("Object count/boundary mismatch")
    return result


def decode(data_folder):
    report = json.loads((data_folder / "report.json").read_text(encoding="utf-8"))
    ccn = (data_folder / "original/application.ccn").read_bytes()
    if report["clickteam"]["build"] != 296:
        raise ValueError("Only the verified build 296 header layout is supported")
    objects = None
    for kind, _, payload in chunks(ccn, 16):
        if kind == 0x2229:
            objects = object_bank(payload)
    if objects is None:
        raise ValueError("Object bank missing")
    by_handle = {obj["handle"]: obj for obj in objects}
    images = {image["handle"] for image in report["images"]}
    references = 0
    for obj in objects:
        used = []
        if "backdrop" in obj:
            used.append(obj["backdrop"]["image"])
        common = obj.get("common", {})
        for animation in common.get("animations", []):
            for direction in animation["directions"]:
                used.extend(direction["images"])
        used.extend(common.get("counter_display", {}).get("images", []))
        if set(used) - images:
            raise ValueError(f"Object {obj['handle']} references missing images {set(used) - images}")
        references += len(used)
    scenes = []
    for frame in report["frames"]:
        scene = dict(index=frame["index"], name=frame["name"], width=frame["width"],
                     height=frame["height"], instances=[])
        for kind, _, payload in chunks((data_folder / "frames" / f"{frame['index']:02d}.ccn-chunks.bin").read_bytes()):
            if kind == 0x3334:
                scene["background_rgb"] = list(payload[8:11])
                scene["flags"] = struct.unpack_from("<I", payload, 12)[0]
            if kind == 0x3338:
                count = struct.unpack_from("<I", payload)[0]
                if 4 + count * 20 > len(payload):
                    raise ValueError("Instance table truncated")
                for i in range(count):
                    handle, object_info, x, y, parent_type, parent_handle, layer, instance = struct.unpack_from(
                        "<HHii4h", payload, 4 + i * 20)
                    if object_info not in by_handle:
                        raise ValueError("Instance references missing object")
                    scene["instances"].append(dict(handle=handle, object_info=object_info,
                        x=x, y=y, parent_type=parent_type, parent_handle=parent_handle, layer=layer, instance=instance))
                scene["instance_trailer_hex"] = payload[4 + count * 20:].hex()
        scenes.append(scene)
    output = data_folder / "scenes"
    output.mkdir(exist_ok=True)
    (output / "objects.json").write_text(json.dumps(objects, ensure_ascii=False, indent=2), encoding="utf-8")
    for scene in scenes:
        (output / f"{scene['index']:02d}.json").write_text(json.dumps(scene, ensure_ascii=False, indent=2), encoding="utf-8")
    summary = dict(apk_sha256=report["apk_sha256"], objects=len(objects), scenes=len(scenes),
                   instances=sum(len(scene["instances"]) for scene in scenes),
                   image_references_checked=references, object_types=dict(Counter(obj["type"] for obj in objects)),
                   status="Original object and layout data decoded; no scene event execution")
    (output / "inventory.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary, indent=2))
    return objects, scenes


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, default=Path("data"))
    decode(parser.parse_args().data)
