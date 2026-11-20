"""Compile verified original scene data and a supported event subset into C tables.

Unsupported expressions are marked invalid. They never become zero-valued
successful conditions. The generated coverage manifest lists every omission.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import struct
from extract_apk import chunks

ROOT = Path(__file__).resolve().parents[1]


class Unsupported(ValueError):
    pass


class NumericParser:
    def __init__(self, payload):
        self.comparison = struct.unpack_from("<H", payload)[0]
        self.tokens = []
        offset = 2
        while offset + 4 <= len(payload):
            typ, op = struct.unpack_from("<hh", payload, offset)
            if (typ, op) == (0, 0):
                if offset + 4 != len(payload):
                    raise Unsupported("expression trailer")
                break
            size = struct.unpack_from("<H", payload, offset + 4)[0]
            if size < 6 or offset + size > len(payload):
                raise Unsupported("expression token boundary")
            self.tokens.append((typ, op, payload[offset + 6:offset + size]))
            offset += size
        else:
            raise Unsupported("missing expression terminator")
        self.pos = 0

    def current(self):
        return self.tokens[self.pos][:2] if self.pos < len(self.tokens) else None

    def expression(self, precedence=0):
        output = self.atom()
        operators = {(0, 2): (1, "EX_ADD"), (0, 4): (1, "EX_SUB"),
                     (0, 6): (2, "EX_MUL"), (0, 8): (2, "EX_DIV")}
        while self.current() in operators:
            rank, instruction = operators[self.current()]
            if rank < precedence:
                break
            self.pos += 1
            output += self.expression(rank + 1)
            output.append((instruction, 0, 0, 0))
        return output

    def atom(self):
        if self.pos >= len(self.tokens):
            raise Unsupported("missing operand")
        typ, op, data = self.tokens[self.pos]
        self.pos += 1
        if (typ, op) == (-1, 0):
            return [("EX_CONST", 0, 0, struct.unpack_from("<i", data)[0])]
        if (typ, op) == (-1, 23):
            return [("EX_CONST", 0, 0, struct.unpack_from("<f", data, 8)[0])]
        if (typ, op) == (-1, 24):
            return [("EX_VAR", -1, struct.unpack_from("<H", data, 4)[0], 0)]
        if (typ, op) == (-4, 0):
            return [("EX_TIMER", 0, 0, 0)]
        if typ >= 2:
            obj = struct.unpack_from("<H", data)[0]
            if (typ, op) == (7, 80):
                return [("EX_COUNTER", obj, 0, 0)]
            if op in (0, 1, 40, 41):
                instruction = {0: "EX_X", 1: "EX_Y", 40: "EX_WIDTH", 41: "EX_HEIGHT"}[op]
                return [(instruction, obj, 0, 0)]
            if op == 16:
                return [("EX_VAR", obj, struct.unpack_from("<H", data, 4)[0], 0)]
        if (typ, op) in ((-1, -1), (-1, 1)):
            result = self.expression()
            if self.current() != (-1, -2):
                raise Unsupported("function/parenthesis close")
            self.pos += 1
            if op == 1:
                result.append(("EX_RANDOM", 0, 0, 0))
            return result
        if typ == -1 and op in (40, 41):
            result = self.expression()
            if self.current() != (-1, -3):
                raise Unsupported("min/max argument separator")
            self.pos += 1
            result += self.expression()
            if self.current() != (-1, -2):
                raise Unsupported("min/max close")
            self.pos += 1
            result.append(("EX_MIN" if op == 40 else "EX_MAX", 0, 0, 0))
            return result
        raise Unsupported(f"numeric expression {typ}:{op}")

    def parse(self):
        code = self.expression()
        if self.pos != len(self.tokens):
            raise Unsupported(f"unconsumed numeric tokens {self.current()}")
        return code


def compile_data(data_folder, destination, frame_indices):
    objects = json.loads((data_folder / "scenes/objects.json").read_text(encoding="utf-8"))
    report = json.loads((data_folder / "report.json").read_text(encoding="utf-8"))
    lines = ["/* Generated from the supplied APK. See data/native/coverage.json. */"]
    globals_initial = []
    for kind, _, payload in chunks((data_folder / "original/application.ccn").read_bytes(), 16):
        if kind == 0x2232:
            count = struct.unpack_from("<H", payload)[0]
            for i in range(count):
                format_code = "<f" if payload[2 + count * 4 + i] == 2 else "<i"
                globals_initial.append(struct.unpack_from(format_code, payload, 2 + i * 4)[0])
    lines.append("static const float initial_globals[] = {%s};" % ",".join(str(float(value)) + "f" for value in globals_initial))
    omissions = Counter()
    expressions = []
    records = []

    def expression(parameter):
        if parameter is None:
            return "NULL"
        name = f"expression_{len(expressions)}"
        try:
            parser = NumericParser(bytes.fromhex(parameter["payload_hex"]))
            code = parser.parse()
            entries = ",".join("{%s,%d,%d,%.9gf}" % (op, obj, index, value) for op, obj, index, value in code)
            # C requires a decimal point or exponent before the float suffix.
            import re
            entries = re.sub(r"(?<=[,])(-?\d+)f(?=})", r"\1.0f", entries)
            lines.append(f"static const Instruction {name}_code[] = {{{entries}}};")
            lines.append(f"static const Expression {name} = {{{name}_code,{len(code)},1,{parser.comparison}}};")
        except (Unsupported, struct.error) as error:
            omissions[str(error)] += 1
            lines.append(f"static const Expression {name} = {{NULL,0,0,0}};")
        expressions.append(name)
        return "&" + name

    def record(item):
        typ, opcode, obj = item["object_type"], item["opcode"], item["object_info"]
        parameters = item["parameters"]
        by_code = {}
        for parameter in parameters:
            by_code.setdefault(parameter["code"], []).append(parameter)
        numeric = [p for p in parameters if p["code"] in (22, 23)]
        a = expression(numeric[0]) if numeric else "NULL"
        b = expression(numeric[1]) if len(numeric) > 1 else "NULL"
        index = 0
        constants = [0] * 6
        for code in (50, 49, 11, 10, 14):
            if code in by_code:
                index = struct.unpack_from("<H", bytes.fromhex(by_code[code][0]["payload_hex"]))[0]
                break
        if 2 in by_code:
            constants[0] = struct.unpack_from("<I", bytes.fromhex(by_code[2][0]["payload_hex"]))[0]
        if 16 in by_code:
            payload = bytes.fromhex(by_code[16][0]["payload_hex"])
            parent, flags, x, y = struct.unpack_from("<HHhh", payload)
            constants[:5] = [parent, flags, x, y, struct.unpack_from("<h", payload, 20)[0]]
        if 26 in by_code:
            constants[0] = struct.unpack_from("<I", bytes.fromhex(by_code[26][0]["payload_hex"]))[0]
        if 39 in by_code:
            constants[0] = struct.unpack_from("<I", bytes.fromhex(by_code[39][0]["payload_hex"]), 4)[0]
        records.append((typ, opcode))
        return "{%d,%d,%d,%d,%d,{%s},%s,%s}" % (typ, opcode, obj, index, item["flags"],
                  ",".join(map(str, constants)), a, b)

    for obj in objects:
        common = obj.get("common", {})
        animations = []
        for anim in common.get("animations", []):
            for direction in anim["directions"]:
                symbol = f"object_{obj['handle']}_a{anim['animation']}_d{direction['direction']}"
                lines.append(f"static const int {symbol}[] = {{{','.join(map(str, direction['images']))}}};")
                animations.append("{%d,%d,%d,%d,%d,%d,%s}" % (anim["animation"], direction["direction"],
                    direction["minimum_speed"], direction["repeat"], direction["back_to"], len(direction["images"]), symbol))
        if animations:
            lines.append(f"static const AnimationDef object_{obj['handle']}_animations[] = {{{','.join(animations)}}};")
        values = common.get("alterable_values", [])
        if values:
            lines.append(f"static const int object_{obj['handle']}_values[] = {{{','.join(map(str, values))}}};")
    lines.append("static const ImageDef images[] = {")
    for image in report["images"]:
        lines.append("{%d,%d,%d,%d,%d}," % (image["handle"], image["width"], image["height"], *image["hotspot"]))
    lines.append("};\nstatic const ObjectDef objects[] = {")
    for obj in objects:
        common = obj.get("common", {})
        animations = sum(len(anim["directions"]) for anim in common.get("animations", []))
        values = common.get("alterable_values", [])
        lines.append("{%d,%d,%d,%d,%d,%d,%d,%s,%s,%d}," % (
            obj["handle"], obj["type"], int(common.get("visible_at_start", True)), int(common.get("create_at_start", True)),
            obj.get("backdrop", {}).get("image", -1), animations, common.get("counter", {}).get("initial", 0),
            f"object_{obj['handle']}_animations" if animations else "NULL",
            f"object_{obj['handle']}_values" if values else "NULL", len(values)))
    lines.append("};")
    scene_entries = []
    for index in frame_indices:
        scene = json.loads((data_folder / "scenes" / f"{index:02d}.json").read_text(encoding="utf-8"))
        lines.append(f"static const InstanceDef scene_{index}_instances[] = {{")
        for instance in scene["instances"]:
            lines.append("{%d,%d,%d,%d,%d}," % (instance["handle"], instance["object_info"], instance["x"], instance["y"], instance["layer"]))
        lines.append("};")
        event_path = data_folder / "events" / f"{index:02d}.json"
        raw_groups = json.loads(event_path.read_text())["groups"] if event_path.exists() else []
        contexts, definitions, compiled = [], [], []
        for group_index, group in enumerate(raw_groups):
            conditions = group["conditions"]
            if len(conditions) == 1 and (conditions[0]["object_type"], conditions[0]["opcode"]) == (-1, -10):
                payload = bytes.fromhex(conditions[0]["parameters"][0]["payload_hex"])
                flags, identity = struct.unpack_from("<HH", payload)
                definitions.append((identity, int(not (flags & 1))))
                contexts.append(identity)
                continue
            if len(conditions) == 1 and (conditions[0]["object_type"], conditions[0]["opcode"]) == (-1, -11):
                if not contexts:
                    raise ValueError("Unbalanced original event groups")
                contexts.pop()
                continue
            prefix = f"scene_{index}_group_{group_index}"
            condition_records = [record(item) for item in conditions]
            action_records = [record(item) for item in group["actions"]]
            lines.append(f"static const EventRecord {prefix}_conditions[] = {{{','.join(condition_records)}}};")
            lines.append(f"static const EventRecord {prefix}_actions[] = {{{','.join(action_records)}}};")
            if contexts:
                lines.append(f"static const int {prefix}_contexts[] = {{{','.join(map(str, contexts))}}};")
            # Child/OR event execution requires parent-selection semantics, not
            # standalone evaluation. Defer it explicitly in this runtime.
            deferred = int(bool(group["flags"] & 0x8000))
            compiled.append("{%s_conditions,%s_actions,%d,%d,%d,%s,%d}" % (
                prefix, prefix, len(conditions), len(group["actions"]), len(contexts),
                prefix + "_contexts" if contexts else "NULL", deferred))
        if contexts:
            raise ValueError("Unclosed original group")
        lines.append(f"static const EventGroup scene_{index}_groups[] = {{{','.join(compiled)}}};")
        ids = ",".join(str(item[0]) for item in definitions) or "0"
        active = ",".join(str(item[1]) for item in definitions) or "0"
        lines.append(f"static const int scene_{index}_ids[] = {{{ids}}};")
        lines.append(f"static const int scene_{index}_active[] = {{{active}}};")
        scene_entries.append("{%d,%d,%d,%d,scene_%d_instances,scene_%d_groups,scene_%d_ids,scene_%d_active,%s}" % (
            index, len(scene["instances"]), len(compiled), len(definitions), index, index, index, index, json.dumps(scene["name"])))
    lines.append(f"static const SceneDef scenes[] = {{{','.join(scene_entries)}}};")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text("\n".join(lines) + "\n", encoding="utf-8")
    coverage = dict(status="Partial scene runtime; full gameplay is not implemented", frames=frame_indices,
                    expressions=len(expressions), unsupported_expressions=dict(omissions),
                    event_records=len(records), record_types={f"{key[0]}:{key[1]}": value for key, value in Counter(records).items()})
    out = data_folder / "native"
    out.mkdir(exist_ok=True)
    (out / "coverage.json").write_text(json.dumps(coverage, indent=2), encoding="utf-8")
    print(f"Generated {destination}; {len(expressions)} expressions, {sum(omissions.values())} explicitly unsupported")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, default=ROOT / "data")
    parser.add_argument("--output", type=Path, default=ROOT / "build/generated_scene_data.h")
    parser.add_argument("--frames", type=int, nargs="+", default=[0, 3, 4, 5])
    args = parser.parse_args()
    compile_data(args.data, args.output, args.frames)
