"""Decode original Android event records into a reviewable intermediate format.

Preserves unimplemented parameter payloads exactly. Does not execute events.
The numeric opcode inventory is the checklist for a future native runtime.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import struct


def parse_record(data, offset, condition):
    header_size = 16 if condition else 14
    if offset + header_size > len(data):
        raise ValueError("Truncated event record")
    size, object_type, opcode, object_info, object_list, flags, other, count, definition = struct.unpack_from(
        "<HhhHhBBBB", data, offset)
    end = offset + size
    if size < header_size or end > len(data):
        raise ValueError("Invalid event record size")
    record = dict(object_type=object_type, opcode=opcode, object_info=object_info,
                  object_list=object_list, flags=flags, other_flags=other,
                  definition=definition, parameters=[])
    if condition:
        record["identifier"] = struct.unpack_from("<H", data, offset + 14)[0]
    pos = offset + header_size
    for _ in range(count):
        length, code = struct.unpack_from("<HH", data, pos)
        if length < 4 or pos + length > end:
            raise ValueError("Invalid event parameter size")
        record["parameters"].append({"code": code, "payload_hex": data[pos + 4:pos + length].hex()})
        pos += length
    if pos != end:
        raise ValueError(f"Unconsumed event data: {end - pos} bytes")
    return record, end


def parse_events(data):
    if not data.startswith(b"ER>>"):
        raise ValueError("Missing event header")
    pos = 4
    maximum_objects, maximum_info, players = struct.unpack_from("<hhh", data, pos)
    pos += 6 + 17 * 2
    qualifiers = struct.unpack_from("<H", data, pos)[0]
    pos += 2 + qualifiers * 4
    result = dict(maximum_objects=maximum_objects, maximum_object_info=maximum_info,
                  players=players, groups=[])
    while pos < len(data):
        tag = data[pos:pos + 4]
        pos += 4
        if tag == b"ERes":
            result["event_count_metadata"] = list(struct.unpack_from("<II", data, pos))
            pos += 8
        elif tag == b"ERop":
            result["option_flags"] = struct.unpack_from("<I", data, pos)[0]
            pos += 4
        elif tag == b"ERev":
            size = struct.unpack_from("<I", data, pos)[0]
            pos += 4
            end = pos + size + 4  # Android stores an extra leading word.
            pos += 4
            if end > len(data):
                raise ValueError("Event groups exceed data boundary")
            while pos < end:
                length, conditions, actions, flags, line, restricted, counter = struct.unpack_from(
                    "<hBBHHii", data, pos)
                group_end = pos - length
                if length >= 0 or group_end > end:
                    raise ValueError("Invalid event group length")
                group = dict(offset=pos, flags=flags, line=line, restricted=restricted,
                             restrict_counter=counter, conditions=[], actions=[])
                pos += 16
                for _ in range(conditions):
                    record, pos = parse_record(data, pos, True)
                    group["conditions"].append(record)
                for _ in range(actions):
                    record, pos = parse_record(data, pos, False)
                    group["actions"].append(record)
                if pos != group_end:
                    raise ValueError("Event group was not consumed exactly")
                result["groups"].append(group)
        elif tag == b"<<ER":
            if pos != len(data):
                raise ValueError("Unexpected bytes after event terminator")
            return result
        else:
            raise ValueError(f"Unknown event section {tag!r} at {pos - 4}")
    raise ValueError("Missing event terminator")


def audit(folder):
    report = json.loads((folder / "report.json").read_text(encoding="utf-8"))
    totals = {"conditions": Counter(), "actions": Counter(), "parameters": Counter()}
    output = folder / "events"
    output.mkdir(exist_ok=True)
    frames = []
    for frame in report["frames"]:
        if not frame["event_bytes"]:
            continue
        parsed = parse_events((folder / "frames" / f"{frame['index']:02d}.events.bin").read_bytes())
        (output / f"{frame['index']:02d}.json").write_text(json.dumps(parsed, indent=2), encoding="utf-8")
        counts = dict(index=frame["index"], name=frame["name"], groups=len(parsed["groups"]), conditions=0, actions=0)
        for group in parsed["groups"]:
            for kind in ("conditions", "actions"):
                counts[kind] += len(group[kind])
                for record in group[kind]:
                    key = f"{record['object_type']}:{record['opcode']}"
                    totals[kind][key] += 1
                    totals["parameters"].update(str(item["code"]) for item in record["parameters"])
        frames.append(counts)
    summary = {"apk_sha256": report["apk_sha256"], "status": "decoded records only; no opcode execution",
               "frames": frames, "opcode_inventory": {key: dict(value.most_common()) for key, value in totals.items()}}
    (output / "inventory.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(f"Decoded {sum(frame['groups'] for frame in frames)} groups in {len(frames)} event-bearing frames")
    print(f"Unique condition opcodes: {len(totals['conditions'])}; actions: {len(totals['actions'])}")
    return summary


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, default=Path("data"))
    audit(parser.parse_args().data)
