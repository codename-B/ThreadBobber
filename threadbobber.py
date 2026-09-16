#!/usr/bin/env python3
"""Compile Yarn Spinner sources with the official compiler and bake the result
into constant tables for ThreadBobber.

Pipeline:  *.yarn (+ .ysls.json declarations) --ysc 3.2.2--> .yarnc (protobuf,
YarnSpinner/yarn_spinner.proto) + -Lines.csv + -Metadata.csv --this tool--> C.

Requires the protobuf Python package and the checked-in yarn_spinner_pb2.py
bindings generated from the pinned upstream schema (see proto/README.md).

Usage: threadbobber.py PROJECT.yarnproject --out DIR [--prefix P]
                    [--profile compact|expanded]
"""

import argparse
import csv
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from google.protobuf.message import DecodeError
from google.protobuf.unknown_fields import UnknownFieldSet

import yarn_spinner_pb2 as yarn

VERSION = "1.0.0"
YSC_VERSION = "3.2.2"


# ---- Yarn Spinner protobuf adapter ------------------------------------------
# Keep the converter's internal operand names independent of upstream casing.
OPERAND_NAMES = {
    "lineID": "line_id",
    "substitutionCount": "substitution_count",
    "commandText": "command_text",
    "hasCondition": "has_condition",
    "functionName": "function_name",
    "variableName": "variable_name",
    "nodeName": "node_name",
    "contentID": "content_id",
    "complexityScore": "complexity_score",
}


def selected_field(message, oneof):
    """Reject unknown operations/values, even alongside a known oneof member."""
    if UnknownFieldSet(message):
        raise CompileError("Unsupported %s field." % message.DESCRIPTOR.name)
    name = message.WhichOneof(oneof)
    if name is None:
        raise CompileError("%s must hold exactly one value." % message.DESCRIPTOR.name)
    return name


def decode_program(blob):
    """Adapt an official .yarnc message to the converter's internal representation."""
    program = yarn.Program()
    try:
        program.ParseFromString(blob)
    except DecodeError as error:
        raise CompileError("Invalid Yarn Spinner protobuf program: %s" % error) from error
    nodes = []
    # Protobuf map iteration order is unspecified. Stable ordering also keeps
    # node IDs, generated tables and program hashes consistent across runtimes.
    for key in sorted(program.nodes):
        node = program.nodes[key]
        instructions = []
        for instruction in node.instructions:
            operation = selected_field(instruction, "InstructionType")
            if operation not in OPCODES:
                raise CompileError("Unsupported instruction: " + operation)
            body = getattr(instruction, operation)
            # A new operand may change execution semantics; never silently drop it.
            if UnknownFieldSet(body):
                raise CompileError("Unsupported operand field for " + operation)
            operands = {
                OPERAND_NAMES.get(field.name, field.name): getattr(body, field.name)
                for field in body.DESCRIPTOR.fields
            }
            instructions.append((operation, operands))
        nodes.append(
            {
                "name": node.name,
                "headers": [(header.key, header.value) for header in node.headers],
                "instructions": instructions,
            }
        )
    initial = {}
    kinds = {"string_value": "string", "bool_value": "bool", "float_value": "number"}
    for key, operand in program.initial_values.items():
        field = selected_field(operand, "value")
        if field not in kinds:
            raise CompileError("Unsupported initial value: " + field)
        initial[key] = (kinds[field], getattr(operand, field))
    return {
        "name": program.name,
        "nodes": nodes,
        "initial": initial,
        "language_version": program.language_version,
    }


# ---- opcode tables -----------------------------------------------------------
# proto instruction -> C opcode
OPCODES = {
    "jumpTo": "ThreadBobberOpcode_JumpTo",
    "peekAndJump": "ThreadBobberOpcode_PeekAndJump",
    "runLine": "ThreadBobberOpcode_RunLine",
    "runCommand": "ThreadBobberOpcode_RunCommand",
    "addOption": "ThreadBobberOpcode_AddOption",
    "showOptions": "ThreadBobberOpcode_ShowOptions",
    "pushString": "ThreadBobberOpcode_PushString",
    "pushFloat": "ThreadBobberOpcode_PushFloat",
    "pushBool": "ThreadBobberOpcode_PushBool",
    "jumpIfFalse": "ThreadBobberOpcode_JumpIfFalse",
    "pop": "ThreadBobberOpcode_Pop",
    "callFunc": "ThreadBobberOpcode_CallFunc",
    "pushVariable": "ThreadBobberOpcode_PushVariable",
    "storeVariable": "ThreadBobberOpcode_StoreVariable",
    "stop": "ThreadBobberOpcode_Stop",
    "runNode": "ThreadBobberOpcode_RunNode",
    "peekAndRunNode": "ThreadBobberOpcode_PeekAndRunNode",
    "detourToNode": "ThreadBobberOpcode_DetourToNode",
    "peekAndDetourToNode": "ThreadBobberOpcode_PeekAndDetourToNode",
    "return": "ThreadBobberOpcode_Return",
    "addSaliencyCandidate": "ThreadBobberOpcode_AddSaliencyCandidate",
    "addSaliencyCandidateFromNode": "ThreadBobberOpcode_AddSaliencyCandidateFromNode",
    "selectSaliencyCandidate": "ThreadBobberOpcode_SelectSaliencyCandidate",
}
OPCODE_VALUE = {
    name: i + 1
    for i, name in enumerate(
        [
            "ThreadBobberOpcode_JumpTo",
            "ThreadBobberOpcode_PeekAndJump",
            "ThreadBobberOpcode_RunLine",
            "ThreadBobberOpcode_RunCommand",
            "ThreadBobberOpcode_AddOption",
            "ThreadBobberOpcode_ShowOptions",
            "ThreadBobberOpcode_PushString",
            "ThreadBobberOpcode_PushFloat",
            "ThreadBobberOpcode_PushBool",
            "ThreadBobberOpcode_JumpIfFalse",
            "ThreadBobberOpcode_Pop",
            "ThreadBobberOpcode_CallFunc",
            "ThreadBobberOpcode_PushVariable",
            "ThreadBobberOpcode_StoreVariable",
            "ThreadBobberOpcode_Stop",
            "ThreadBobberOpcode_RunNode",
            "ThreadBobberOpcode_PeekAndRunNode",
            "ThreadBobberOpcode_DetourToNode",
            "ThreadBobberOpcode_PeekAndDetourToNode",
            "ThreadBobberOpcode_Return",
            "ThreadBobberOpcode_EvaluateVariable",
            "ThreadBobberOpcode_AddSaliencyCandidate",
            "ThreadBobberOpcode_SelectSaliencyCandidate",
        ]
    )
}
WIDTH = {
    "Elided": 0,
    "ThreadBobberOpcode_AddSaliencyCandidate": 7,
    "ThreadBobberOpcode_SelectSaliencyCandidate": 1,
    "ThreadBobberOpcode_EvaluateVariable": 3,
    "ThreadBobberOpcode_JumpTo": 3,
    "ThreadBobberOpcode_PeekAndJump": 1,
    "ThreadBobberOpcode_RunLine": 5,
    "ThreadBobberOpcode_RunCommand": 3,
    "ThreadBobberOpcode_AddOption": 7,
    "ThreadBobberOpcode_ShowOptions": 1,
    "ThreadBobberOpcode_PushString": 3,
    "ThreadBobberOpcode_PushFloat": 5,
    "ThreadBobberOpcode_PushBool": 2,
    "ThreadBobberOpcode_JumpIfFalse": 3,
    "ThreadBobberOpcode_Pop": 1,
    "ThreadBobberOpcode_CallFunc": 3,
    "ThreadBobberOpcode_PushVariable": 3,
    "ThreadBobberOpcode_StoreVariable": 3,
    "ThreadBobberOpcode_Stop": 1,
    "ThreadBobberOpcode_RunNode": 3,
    "ThreadBobberOpcode_PeekAndRunNode": 1,
    "ThreadBobberOpcode_DetourToNode": 3,
    "ThreadBobberOpcode_PeekAndDetourToNode": 1,
    "ThreadBobberOpcode_Return": 1,
}
# Yarn standard library subset: name -> (YFN id, argc, return type)
BUILTINS = {
    "String.Add": ("ThreadBobberFunction_StringAdd", 2, "string"),
    "string": ("ThreadBobberFunction_ConvertString", 1, "string"),
    "number": ("ThreadBobberFunction_ConvertNumber", 1, "number"),
    "bool": ("ThreadBobberFunction_ConvertBoolean", 1, "bool"),
    "has_any_content": ("ThreadBobberFunction_HasAnyContent", 1, "bool"),
    "round_places": ("ThreadBobberFunction_RoundPlaces", 2, "number"),
    "random_range_float": ("ThreadBobberFunction_RandomRangeFloat", 2, "number"),
    "Enum.EqualTo": ("ThreadBobberFunction_ValueEqualTo", 2, "bool"),
    "Enum.NotEqualTo": ("ThreadBobberFunction_ValueNotEqualTo", 2, "bool"),
    "Number.Add": ("ThreadBobberFunction_NumberAdd", 2, "number"),
    "Number.Minus": ("ThreadBobberFunction_NumberMinus", 2, "number"),
    "Number.Multiply": ("ThreadBobberFunction_NumberMultiply", 2, "number"),
    "Number.Divide": ("ThreadBobberFunction_NumberDivide", 2, "number"),
    "Number.Modulo": ("ThreadBobberFunction_NumberModulo", 2, "number"),
    "Number.UnaryMinus": ("ThreadBobberFunction_NumberUnaryMinus", 1, "number"),
    "Number.EqualTo": ("ThreadBobberFunction_NumberEqualTo", 2, "bool"),
    "Number.NotEqualTo": ("ThreadBobberFunction_NumberNotEqualTo", 2, "bool"),
    "Number.GreaterThan": ("ThreadBobberFunction_NumberGreaterThan", 2, "bool"),
    "Number.LessThan": ("ThreadBobberFunction_NumberLessThan", 2, "bool"),
    "Number.GreaterThanOrEqualTo": ("ThreadBobberFunction_NumberGreaterThanOrEqualTo", 2, "bool"),
    "Number.LessThanOrEqualTo": ("ThreadBobberFunction_NumberLessThanOrEqualTo", 2, "bool"),
    "Bool.And": ("ThreadBobberFunction_BoolAnd", 2, "bool"),
    "Bool.Or": ("ThreadBobberFunction_BoolOr", 2, "bool"),
    "Bool.Xor": ("ThreadBobberFunction_BoolXor", 2, "bool"),
    "Bool.Not": ("ThreadBobberFunction_BoolNot", 1, "bool"),
    "Bool.EqualTo": ("ThreadBobberFunction_BoolEqualTo", 2, "bool"),
    "Bool.NotEqualTo": ("ThreadBobberFunction_BoolNotEqualTo", 2, "bool"),
    "String.EqualTo": ("ThreadBobberFunction_StringEqualTo", 2, "bool"),
    "String.NotEqualTo": ("ThreadBobberFunction_StringNotEqualTo", 2, "bool"),
    "visited": ("ThreadBobberFunction_Visited", 1, "bool"),
    "visited_count": ("ThreadBobberFunction_VisitedCount", 1, "number"),
    "random": ("ThreadBobberFunction_Random", 0, "number"),
    "random_range": ("ThreadBobberFunction_RandomRange", 2, "number"),
    "dice": ("ThreadBobberFunction_Dice", 1, "number"),
    "min": ("ThreadBobberFunction_Min", 2, "number"),
    "max": ("ThreadBobberFunction_Max", 2, "number"),
    "round": ("ThreadBobberFunction_Round", 1, "number"),
    "floor": ("ThreadBobberFunction_Floor", 1, "number"),
    "ceil": ("ThreadBobberFunction_Ceil", 1, "number"),
    "inc": ("ThreadBobberFunction_Inc", 1, "number"),
    "dec": ("ThreadBobberFunction_Dec", 1, "number"),
    "int": ("ThreadBobberFunction_Int", 1, "number"),
    "decimal": ("ThreadBobberFunction_Decimal", 1, "number"),
}
UNSUPPORTED_FUNCS = {
    "format": "format()",
    "format_invariant": "format_invariant()",
}
TYPE_ENUM = {"number": 1, "bool": 2, "string": 3, "none": 0}


class CompileError(Exception):
    pass


def fnv1a(data, h=2166136261):
    for c in data:
        h = ((h ^ c) * 16777619) & 0xFFFFFFFF
    return h


def c_identifier(name):
    s = re.sub(r"[^A-Za-z0-9_]", "_", name)
    return ("_" + s) if s[:1].isdigit() else s


def c_identifiers(names, reserved=()):
    """Keep existing spellings where possible and disambiguate generated symbols."""
    bases = {name: c_identifier(name) for name in names}
    blocked = set(bases.values()) | set(reserved)
    used = set(reserved)
    result = {}
    for name, base in bases.items():
        symbol = base
        suffix = 2
        if symbol in used:
            symbol = "%s_%d" % (base, suffix)
            while symbol in blocked or symbol in used:
                suffix += 1
                symbol = "%s_%d" % (base, suffix)
        used.add(symbol)
        result[name] = symbol
    return result


def split_command(text):
    """Split command text into tokens respecting double quotes. Returns (name, [(token, quoted)])."""
    toks, cur, quoted, inq, i = [], "", False, False, 0
    while i < len(text):
        c = text[i]
        if inq:
            if c == "\\" and i + 1 < len(text):
                cur += text[i + 1]
                i += 2
                continue
            if c == '"':
                inq = False
                i += 1
                continue
            cur += c
        elif c == '"':
            inq = True
            quoted = True
        elif c.isspace():
            if cur or quoted:
                toks.append((cur, quoted))
                cur, quoted = "", False
        else:
            cur += c
        i += 1
    if inq:
        raise CompileError("unterminated quote in command <<%s>>" % text)
    if cur or quoted:
        toks.append((cur, quoted))
    if not toks:
        raise CompileError("empty command")
    return toks[0][0], toks[1:]


class StringTable:
    def __init__(self):
        self.ids, self.items = {}, []

    def add(self, s):
        if s not in self.ids:
            if "\0" in s:
                raise CompileError("Strings must not contain null characters.")
            if (
                len(s.encode("utf-8")) > 65534
            ):  # pool offsets are 16-bit; lines are checked against --max-line-length
                raise CompileError("string longer than 65534 bytes: %r" % s[:40])
            self.ids[s] = len(self.items)
            self.items.append(s)
        return self.ids[s]


def run_ysc(project, workdir):
    """Compile a project using the pinned official Yarn Spinner compiler."""
    ysc = shutil.which("ysc")
    if not ysc:
        raise CompileError("ysc was not found. Install YarnSpinner.Console " + YSC_VERSION + ".")
    version = subprocess.run(
        [ysc, "--version"], capture_output=True, text=True, encoding="utf-8", timeout=60
    )
    reported = version.stdout.strip()
    if version.returncode or reported.split("+")[0] != YSC_VERSION:
        raise CompileError("Expected ysc %s; found %r." % (YSC_VERSION, reported))
    compiled = subprocess.run(
        [ysc, "compile", os.path.abspath(project), "-o", workdir, "-n", "program"],
        capture_output=True,
        text=True,
        encoding="utf-8",
        timeout=120,
    )
    if compiled.returncode or not os.path.isfile(os.path.join(workdir, "program.yarnc")):
        raise CompileError(
            "ysc could not compile the project:\n" + compiled.stdout + compiled.stderr
        )
    return YSC_VERSION


def load_definitions(project, definitions=None):
    if definitions:
        return read_definitions([os.path.abspath(definitions)])
    if project.endswith(".yarnc"):
        return {}, {}
    with open(project, encoding="utf-8-sig") as f:
        proj = json.load(f)
    defs = proj.get("definitions")
    if not defs:
        return {}, {}
    paths = defs if isinstance(defs, list) else [defs]
    return read_definitions(
        [os.path.join(os.path.dirname(os.path.abspath(project)), rel) for rel in paths]
    )


def read_definitions(paths):
    """Read command and function declarations shared with the Yarn language server."""
    commands, functions = {}, {}
    for path in paths:
        with open(path, encoding="utf-8-sig") as f:
            d = json.load(f)
        for c in d.get("Commands", []):
            if c["YarnName"] in commands:
                raise CompileError("Duplicate command declaration: " + c["YarnName"])
            commands[c["YarnName"]] = {
                "id": len(commands),
                "params": [p["Type"] for p in c.get("Parameters", [])],
                "async": bool(c.get("Async", False)),
            }
        for fn in d.get("Functions", []):
            if fn["YarnName"] in functions:
                raise CompileError("Duplicate function declaration: " + fn["YarnName"])
            functions[fn["YarnName"]] = {
                "id": len(functions),
                "params": [p["Type"] for p in fn.get("Parameters", [])],
                "returns": fn["ReturnType"],
            }
    if len(commands) > 256 or len(functions) > 256:
        raise CompileError("A library may declare at most 256 commands and 256 functions.")
    return commands, functions


def compile_program(args):

    def read_csv(path):
        with open(path, encoding="utf-8-sig", newline="") as stream:
            return list(csv.DictReader(stream))

    if args.project.endswith(".yarnc"):
        if not args.lines:
            raise CompileError(
                "Converting .yarnc requires --lines with the official string-table CSV."
            )
        blob = Path(args.project).read_bytes()
        lines_csv = read_csv(args.lines)
        meta_csv = read_csv(args.metadata) if args.metadata else []
        ysc_version = YSC_VERSION
    else:
        with tempfile.TemporaryDirectory(prefix="threadbobber-") as work:
            ysc_version = run_ysc(args.project, work)
            blob = Path(work, "program.yarnc").read_bytes()
            lines_csv = read_csv(os.path.join(work, "program-Lines.csv"))
            meta_csv = read_csv(os.path.join(work, "program-Metadata.csv"))
    prog = decode_program(blob)
    if not prog["nodes"]:
        raise CompileError("The compiled program contains no nodes.")
    if prog["language_version"] != 4:
        raise CompileError(
            "Expected the program version emitted by ysc 3.2.2 (4); found %s."
            % prog["language_version"]
        )

    def boolean(value):
        return ("pushBool", {"value": bool(value)})

    def variable_instruction(name):
        return ("pushVariable", {"variable_name": name})

    def combine(name):
        return [("pushFloat", {"value": 2.0}), ("callFunc", {"function_name": name})]

    def conditions(node):
        result = [boolean(True)]
        for name in (
            dict(node["headers"]).get("$Yarn.Internal.ContentSaliencyVariables", "").split(";")
        ):
            if name:
                result += [variable_instruction(name)] + combine("Bool.And")
        return result

    groups = {}
    for node in prog["nodes"]:
        group = dict(node["headers"]).get("$Yarn.Internal.NodeGroup")
        if group:
            groups.setdefault(group, []).append(node)
    for group, members in groups.items():
        query = [boolean(False)]
        for member in members:
            query += conditions(member) + combine("Bool.Or")
        prog["nodes"].append(
            {
                "name": "$ThreadBobber.HasAnyContent." + group,
                "headers": [("tags", "Yarn.SmartVariable")],
                "instructions": query,
            }
        )
    commands_decl, functions_decl = load_definitions(args.project, args.definitions)
    line_text = {r["id"]: r["text"] for r in lines_csv}
    line_tags = {r["id"]: r.get("tags", "") for r in meta_csv}
    pool = StringTable()
    pool.add("")
    errors, warnings = [], []

    # Variables (declared + internal tracking/once variables) come from initial_values.
    var_names = sorted(prog["initial"].keys(), key=lambda n: (n.startswith("$Yarn.Internal"), n))
    if len(var_names) > args.max_vars:
        errors.append(
            "%d variables exceed the runtime limit of %d" % (len(var_names), args.max_vars)
        )
    var_index = {n: i for i, n in enumerate(var_names)}
    variables = []
    for n in var_names:
        t, v = prog["initial"][n]
        init = {
            "number": lambda: struct.unpack("<I", struct.pack("<f", v))[0],
            "bool": lambda: 1 if v else 0,
            "string": lambda: pool.add(v),
        }[t]()
        variables.append({"name": n, "type": t, "init": init, "hash": fnv1a(n.encode("utf-8"))})

    node_index = {n["name"]: i for i, n in enumerate(prog["nodes"])}
    if len(node_index) != len(prog["nodes"]):
        raise CompileError("Duplicate node names are not supported.")
    for name in node_index:
        pool.add(name)

    lines, line_ids = [], {}

    def line_slot(lid):
        if lid not in line_ids:
            if lid not in line_text:
                raise CompileError("line %s missing from string table" % lid)
            text = line_text[lid]
            if "[" in text or "]" in text:
                raise CompileError("line %s uses markup ([...]) which is unsupported" % lid)
            if len(text) > args.max_line_length:
                raise CompileError(
                    "line %s is %d characters (limit %d)" % (lid, len(text), args.max_line_length)
                )
            subs = set(int(m) for m in re.findall(r"\{(\d+)\}", text))
            if subs and max(subs) >= args.max_substitutions:
                raise CompileError("Line %s exceeds the substitution capacity." % lid)
            line_ids[lid] = len(lines)
            lines.append(
                {
                    "id": lid,
                    "text": pool.add(text),
                    "id_str": pool.add(lid),
                    "tags": line_tags.get(lid, ""),
                }
            )
        return line_ids[lid]

    funcs, func_slots = [], {}

    def func_slot(name):
        if name in func_slots:
            return func_slots[name]
        if name in BUILTINS:
            b, argc, ret = BUILTINS[name]
            entry = {"name": name, "builtin": b, "host": 0, "argc": argc, "ret": ret}
        elif name in functions_decl:
            d = functions_decl[name]
            if len(d["params"]) > args.max_parameters:
                raise CompileError("Function %s exceeds the parameter capacity." % name)
            if d["returns"] not in ("number", "bool", "string"):
                raise CompileError("function %s must return number, bool or string" % name)
            entry = {
                "name": name,
                "builtin": "ThreadBobberFunction_None",
                "host": d["id"],
                "argc": len(d["params"]),
                "ret": d["returns"],
            }
        elif name in UNSUPPORTED_FUNCS:
            raise CompileError("function %s: %s unsupported" % (name, UNSUPPORTED_FUNCS[name]))
        elif name.startswith("Enum."):
            raise CompileError("function %s: enums are unsupported" % name)
        else:
            raise CompileError(
                "function %s is neither a supported builtin nor declared in the project .ysls.json"
                % name
            )
        func_slots[name] = len(funcs)
        funcs.append(entry)
        return func_slots[name]

    commands = []
    sites = {}

    def command_slot(text, node_name, substitutions):
        name, toks = split_command(text)
        if substitutions > args.max_substitutions:
            raise CompileError("Command substitutions exceed the configured capacity.")
        if substitutions or name not in commands_decl:
            site = sites.get(node_name, 0)
            if site >= 256:
                raise CompileError("A node may contain at most 256 command sites.")
            sites[node_name] = site + 1
            commands.append(
                {
                    "name": name,
                    "host": 0,
                    "args": [],
                    "site": site,
                    "text": text,
                    "is_text": 1,
                    "text_id": pool.add(text),
                    "substitutions": substitutions,
                }
            )
            return len(commands) - 1
        if name not in commands_decl:
            raise CompileError(
                "node %s: command <<%s>> is not declared in the project .ysls.json"
                % (node_name, text)
            )
        d = commands_decl[name]
        if len(toks) != len(d["params"]):
            raise CompileError(
                "node %s: <<%s>> expects %d arguments" % (node_name, text, len(d["params"]))
            )
        if len(toks) > args.max_parameters:
            raise CompileError("Command %s exceeds the parameter capacity." % name)
        vals = []
        for (tok, quoted), ptype in zip(toks, d["params"]):
            if ptype == "number":
                try:
                    v = float(tok)
                except ValueError:
                    raise CompileError(
                        "node %s: <<%s>> argument %r is not a number" % (node_name, text, tok)
                    )
                vals.append((1, struct.unpack("<I", struct.pack("<f", v))[0]))
            elif ptype == "bool":
                if tok.lower() not in ("true", "false"):
                    raise CompileError(
                        "node %s: <<%s>> argument %r is not a bool" % (node_name, text, tok)
                    )
                vals.append((2, 1 if tok.lower() == "true" else 0))
            elif ptype == "string":
                vals.append((3, pool.add(tok)))
            else:
                raise CompileError("command %s: parameter type %s unsupported" % (name, ptype))
        site = sites.get(node_name, 0)
        if site >= 256:
            raise CompileError("node %s has more than 256 command sites" % node_name)
        sites[node_name] = site + 1
        commands.append(
            {
                "name": name,
                "host": d["id"],
                "args": vals,
                "site": site,
                "text": text,
                "is_text": 0,
                "text_id": pool.add(text),
                "substitutions": 0,
            }
        )
        return len(commands) - 1

    code = bytearray()
    nodes_out = []
    for n in prog["nodes"]:
        try:
            # First pass: instruction widths -> byte offsets.
            ops = []
            original_starts = []
            pending_candidates = 0
            expanded_instructions = []
            for kind, body in n["instructions"]:
                original_starts.append(len(expanded_instructions))
                if kind == "addSaliencyCandidateFromNode":
                    target_name = body["node_name"]
                    if target_name not in node_index:
                        raise CompileError("Missing saliency node: " + target_name)
                    target = prog["nodes"][node_index[target_name]]
                    score = int(
                        dict(target["headers"]).get("$Yarn.Internal.ContentSaliencyComplexity", "0")
                    )
                    expanded_instructions += conditions(target)
                    expanded_instructions.append(
                        (
                            "addSaliencyCandidate",
                            {
                                "content_id": target_name,
                                "complexity_score": score,
                                "destination": body["destination"],
                            },
                        )
                    )
                else:
                    expanded_instructions.append((kind, body))
            original_starts.append(len(expanded_instructions))
            for kind, body in expanded_instructions:
                op = OPCODES[kind]
                if op == "ThreadBobberOpcode_PushVariable":
                    variable = body["variable_name"]
                    if variable not in var_index and variable in node_index:
                        target = prog["nodes"][node_index[variable]]
                        if "Yarn.SmartVariable" in dict(target["headers"]).get("tags", "").split():
                            op = "ThreadBobberOpcode_EvaluateVariable"
                ops.append((op, body))
            # Literal content queries can enter their generated expression directly.
            # Keep instruction positions for branch relocation, and elide the two
            # argument-stack operations. Dynamic names retain the library call.
            branch_targets = set()
            for _, body in n["instructions"]:
                target = body.get("destination")
                if target is not None and 0 <= target < len(original_starts):
                    branch_targets.add(original_starts[target])
            for index in range(2, len(ops)):
                op, body = ops[index]
                if (
                    op != "ThreadBobberOpcode_CallFunc"
                    or body["function_name"] != "has_any_content"
                ):
                    continue
                if (
                    ops[index - 1][0] != "ThreadBobberOpcode_PushFloat"
                    or ops[index - 2][0] != "ThreadBobberOpcode_PushString"
                ):
                    continue
                if ops[index - 1][1]["value"] != 1:
                    continue
                if index in branch_targets or index - 1 in branch_targets:
                    continue
                name = ops[index - 2][1]["value"]
                query = "$ThreadBobber.HasAnyContent." + name
                if query in node_index:
                    ops[index - 2] = (
                        "ThreadBobberOpcode_EvaluateVariable",
                        {"variable_name": query},
                    )
                else:
                    ops[index - 2] = ("ThreadBobberOpcode_PushBool", {"value": name in node_index})
                ops[index - 1] = ops[index] = ("Elided", {})
            offsets = [0]
            for op, _ in ops:
                offsets.append(offsets[-1] + WIDTH[op])
            if offsets[-1] > 65535:
                raise CompileError("node %s exceeds 64 KiB of bytecode" % n["name"])

            def dest(idx):
                if not 0 <= idx < len(original_starts):
                    raise CompileError(
                        "node %s: jump destination %d out of range" % (n["name"], idx)
                    )
                return offsets[original_starts[idx]]

            start = len(code)
            pending_options = 0
            for op, body in ops:
                if op == "Elided":
                    continue
                code.append(OPCODE_VALUE[op])
                if op in ("ThreadBobberOpcode_JumpTo", "ThreadBobberOpcode_JumpIfFalse"):
                    code += struct.pack("<H", dest(body["destination"]))
                elif op == "ThreadBobberOpcode_RunLine":
                    subs = body["substitution_count"]
                    if subs > args.max_substitutions:
                        raise CompileError(
                            "node %s: line with %d substitutions" % (n["name"], subs)
                        )
                    code += struct.pack("<HH", line_slot(body["line_id"]), subs)
                elif op == "ThreadBobberOpcode_RunCommand":
                    slot = command_slot(
                        body["command_text"], n["name"], body["substitution_count"]
                    )
                    code += struct.pack("<H", slot)
                elif op == "ThreadBobberOpcode_AddOption":
                    subs = body["substitution_count"]
                    if subs > args.max_substitutions:
                        raise CompileError(
                            "node %s: option with %d substitutions" % (n["name"], subs)
                        )
                    pending_options += 1
                    if pending_options > args.max_options:
                        raise CompileError(
                            "node %s: more than %d options in one group"
                            % (n["name"], args.max_options)
                        )
                    code += struct.pack(
                        "<HHH",
                        line_slot(body["line_id"]),
                        dest(body["destination"]),
                        subs | (0x100 if body["has_condition"] else 0),
                    )
                elif op == "ThreadBobberOpcode_ShowOptions":
                    pending_options = 0
                elif op == "ThreadBobberOpcode_AddSaliencyCandidate":
                    content = body["content_id"]
                    name = "$Yarn.Internal.Content.ViewCount." + content
                    if name not in var_index:
                        var_index[name] = len(variables)
                        variables.append(
                            {
                                "name": name,
                                "type": "number",
                                "init": 0,
                                "hash": fnv1a(name.encode("utf-8")),
                            }
                        )
                    score = body["complexity_score"]
                    if not 0 <= score <= 65535:
                        raise CompileError("Saliency complexity exceeds 16 bits.")
                    pending_candidates += 1
                    if pending_candidates > 8:
                        raise CompileError("A saliency selection exceeds eight candidates.")
                    code += struct.pack("<HHH", var_index[name], dest(body["destination"]), score)
                elif op == "ThreadBobberOpcode_SelectSaliencyCandidate":
                    pending_candidates = 0
                elif op == "ThreadBobberOpcode_EvaluateVariable":
                    code += struct.pack("<H", node_index[body["variable_name"]])
                elif op == "ThreadBobberOpcode_PushString":
                    code += struct.pack("<H", pool.add(body["value"]))
                elif op == "ThreadBobberOpcode_PushFloat":
                    code += struct.pack("<f", body["value"])
                elif op == "ThreadBobberOpcode_PushBool":
                    code.append(1 if body["value"] else 0)
                elif op == "ThreadBobberOpcode_CallFunc":
                    code += struct.pack("<H", func_slot(body["function_name"]))
                elif op in ("ThreadBobberOpcode_PushVariable", "ThreadBobberOpcode_StoreVariable"):
                    vn = body["variable_name"]
                    if vn not in var_index:
                        raise CompileError(
                            "node %s: variable %s has no declaration / initial value"
                            % (n["name"], vn)
                        )
                    code += struct.pack("<H", var_index[vn])
                elif op in ("ThreadBobberOpcode_RunNode", "ThreadBobberOpcode_DetourToNode"):
                    target = body["node_name"]
                    if target not in node_index:
                        raise CompileError("node %s: jump to unknown node %s" % (n["name"], target))
                    code += struct.pack("<H", node_index[target])
            tracking = dict(n["headers"]).get("$Yarn.Internal.TrackingVariable")
            nodes_out.append(
                {
                    "name": n["name"],
                    "start": start,
                    "length": len(code) - start,
                    "tracking": var_index[tracking] if tracking else 0xFFFF,
                    "headers": n["headers"],
                }
            )
        except CompileError as e:
            errors.append(str(e))
    if len(variables) > args.max_vars:
        errors.append("Stored variables and saliency history exceed the variable capacity.")
    if len(code) > 65535:
        errors.append("program bytecode exceeds 64 KiB")
    for variable in variables:
        pool.add(variable["name"])
    pool_bytes = b"".join(text.encode("utf-8") + b"\0" for text in pool.items)
    if len(pool_bytes) > (65535 if args.profile == "compact" else 0xFFFFFFFF):
        errors.append("The string table exceeds this profile's addressable size.")
    if any(
        len(table) > 65535 for table in (pool.items, nodes_out, variables, lines, commands, funcs)
    ):
        errors.append("A program table exceeds the 16-bit instruction operand limit.")
    for w in sorted(set(warnings)):
        print("warning:", w, file=sys.stderr)
    if errors:
        for e in errors:
            print("error:", e, file=sys.stderr)
        raise CompileError("%d error(s); nothing written" % len(errors))

    # Include every table that changes execution or presentation in the program identity.
    identity = [list(code), pool.items, nodes_out, variables, lines, funcs, commands]
    h = fnv1a(
        json.dumps(identity, sort_keys=True, ensure_ascii=True, separators=(",", ":")).encode(
            "ascii"
        )
    )
    write_outputs(
        args,
        prog,
        ysc_version,
        pool,
        code,
        nodes_out,
        variables,
        lines,
        funcs,
        commands,
        commands_decl,
        functions_decl,
        h,
    )


def c_string(s):
    out = ""
    for ch in s:
        o = ord(ch)
        if ch == '"':
            out += '\\"'
        elif ch == "\\":
            out += "\\\\"
        elif ch == "?":
            out += "\\?"
        elif ch == "\n":
            out += "\\n"
        elif 32 <= o < 127:
            out += ch
        else:
            out += "".join("\\%03o" % b for b in ch.encode("utf-8"))
    return out


def write_outputs(
    args, prog, ysc_version, pool, code, nodes, variables, lines, funcs, commands, cdecl, fdecl, h
):
    prefix = args.prefix
    upper_prefix = prefix.upper()
    for v in variables:
        pool.add(v["name"])
    offsets, pos = [], 0
    for s in pool.items:
        offsets.append(pos)
        pos += len(s.encode("utf-8")) + 1
    banner = "/* Generated by threadbobber.py from %s using ysc %s. Do not edit. */" % (
        os.path.basename(args.project),
        ysc_version,
    )
    hdr = [
        banner,
        "#ifndef %s_PROGRAM_H" % upper_prefix,
        "#define %s_PROGRAM_H" % upper_prefix,
        '#include "threadbobber.h"',
        "extern const ThreadBobberProgram %s_program;" % prefix,
        "enum { %s_VAR_COUNT=%d, %s_NODE_COUNT=%d, %s_LINE_COUNT=%d, %s_COMMAND_COUNT=%d };"
        % (
            upper_prefix,
            len(variables),
            upper_prefix,
            len(nodes),
            upper_prefix,
            len(lines),
            upper_prefix,
            len(commands),
        ),
        "#define %s_PROGRAM_HASH 0x%08xu" % (upper_prefix, h),
        '#define %s_YSC_VERSION "%s"' % (upper_prefix, ysc_version.split("+")[0]),
        "#if THREADBOBBER_VERSION_MAJOR != %s" % VERSION.split(".")[0],
        '#error "This program was generated for ThreadBobber %s."' % VERSION,
        "#endif",
    ]
    public = [(i, v) for i, v in enumerate(variables) if not v["name"].startswith("$Yarn.Internal")]
    variable_symbols = c_identifiers(v["name"][1:] for _, v in public)
    node_symbols = c_identifiers((n["name"] for n in nodes), ("COUNT",))
    if public:
        hdr.append(
            "enum { "
            + ", ".join(
                "%s_VARIABLE_%s=%d" % (upper_prefix, variable_symbols[v["name"][1:]], i)
                for i, v in public
            )
            + " };"
        )
    hdr.append(
        "enum { "
        + ", ".join(
            "%s_NODE_%s=%d" % (upper_prefix, node_symbols[n["name"]], i)
            for i, n in enumerate(nodes)
        )
        + " };"
    )
    tagged = [
        (i, line)
        for i, line in enumerate(lines)
        if not re.fullmatch(r"line:[0-9a-f]{7,8}", line["id"])
    ]
    if tagged:
        line_symbols = c_identifiers((line["id"].split(":", 1)[1] for _, line in tagged), ("COUNT",))
        hdr.append(
            "enum { "
            + ", ".join(
                "%s_LINE_%s=%d" % (upper_prefix, line_symbols[line["id"].split(":", 1)[1]], i)
                for i, line in tagged
            )
            + " };"
        )
    if cdecl:
        command_symbols = c_identifiers(cdecl, ("COUNT", "DECLARATION_COUNT"))
        hdr.append(
            "enum { "
            + ", ".join(
                "%s_COMMAND_%s=%d" % (upper_prefix, command_symbols[k], v["id"])
                for k, v in cdecl.items()
            )
            + ", %s_COMMAND_DECLARATION_COUNT=%d };" % (upper_prefix, len(cdecl))
        )
    if fdecl:
        function_symbols = c_identifiers(fdecl, ("COUNT",))
        hdr.append(
            "enum { "
            + ", ".join(
                "%s_FUNCTION_%s=%d" % (upper_prefix, function_symbols[k], v["id"])
                for k, v in fdecl.items()
            )
            + ", %s_FUNCTION_COUNT=%d };" % (upper_prefix, len(fdecl))
        )
    hdr += [
        "#if THREADBOBBER_CANDIDATES < 8 || THREADBOBBER_MAX_VARS < %d || THREADBOBBER_OPTIONS < %d || THREADBOBBER_SUBSTITUTIONS < %d || THREADBOBBER_COMMAND_ARGS < %d"
        % (len(variables), args.max_options, args.max_substitutions, args.max_parameters),
        '#error "This program requires a larger ThreadBobber capacity profile."',
        "#endif",
    ]
    if args.profile == "expanded":
        hdr += [
            "#ifndef THREADBOBBER_PROFILE_EXPANDED",
            '#error "This program requires the expanded profile."',
            "#endif",
        ]
    # A generated program is a C symbol when included by a C++ consumer.
    declaration = "extern const ThreadBobberProgram %s_program;" % prefix
    index = hdr.index(declaration)
    hdr[index : index + 1] = [
        "#ifdef __cplusplus",
        'extern "C" {',
        "#endif",
        declaration,
        "#ifdef __cplusplus",
        "}",
        "#endif",
    ]
    hdr += ["#endif", ""]
    src = [banner, '#include "%s_program.h"' % prefix, "static const char %s_strings[]=" % prefix]
    src += ['"%s' % c_string(s) + chr(92) + '0"' for s in pool.items]
    src[-1] += ";"
    src.append(
        "static const ThreadBobberId %s_string_offsets[%d]={%s};"
        % (prefix, len(offsets), ",".join(str(o) for o in offsets))
    )
    src.append(
        "static const uint8_t %s_code[%d]={%s};"
        % (prefix, max(len(code), 1), ",".join(str(b) for b in code) or "0")
    )
    src.append(
        "static const ThreadBobberNode %s_nodes[%d]={%s};"
        % (
            prefix,
            len(nodes),
            ",".join(
                "{%d,%d,%d,%s,%d}"
                % (
                    pool.ids[n["name"]],
                    n["start"],
                    n["length"],
                    "THREADBOBBER_NONE" if n["tracking"] == 65535 else str(n["tracking"]),
                    int("Yarn.SmartVariable" in dict(n["headers"]).get("tags", "").split()),
                )
                for n in nodes
            ),
        )
    )
    src.append(
        "static const ThreadBobberVariable %s_vars[%d]={%s};"
        % (
            prefix,
            max(len(variables), 1),
            ",".join(
                "{%d,%d,0x%08xu,%du}"
                % (pool.ids[v["name"]], TYPE_ENUM[v["type"]], v["hash"], v["init"])
                for v in variables
            )
            or "{0,1,0,0}",
        )
    )
    src.append(
        "static const ThreadBobberLine %s_lines[%d]={%s};"
        % (
            prefix,
            max(len(lines), 1),
            ",".join("{%d,%d}" % (line["text"], line["id_str"]) for line in lines) or "{0,0}",
        )
    )

    def argfmt(vals):
        vals = list(vals) + [(0, 0)] * (4 - len(vals))
        return "{" + ",".join("{%d,%du}" % v for v in vals) + "}"

    src.append(
        "static const ThreadBobberCommand %s_commands[%d]={%s};"
        % (
            prefix,
            max(len(commands), 1),
            ",".join(
                "{%d,%d,%d,%d,%s,%d,%d}"
                % (
                    c["host"],
                    len(c["args"]),
                    c["site"],
                    c["is_text"],
                    argfmt(c["args"]),
                    c["text_id"],
                    c["substitutions"],
                )
                for c in commands
            )
            or "{0,0,0,0,{{0,0},{0,0},{0,0},{0,0}},0,0}",
        )
    )
    src.append(
        "static const ThreadBobberFunction %s_funcs[%d]={%s};"
        % (
            prefix,
            max(len(funcs), 1),
            ",".join(
                "{%s,%d,%d,%d}" % (f["builtin"], f["host"], f["argc"], TYPE_ENUM[f["ret"]])
                for f in funcs
            )
            or "{0,0,0,0}",
        )
    )
    src.append(
        "const ThreadBobberProgram %s_program={%s_strings,%s_string_offsets,%d,%s_code,%d,%s_nodes,%d,%s_vars,%d,%s_lines,%d,%s_commands,%d,%s_funcs,%d,0x%08xu};"
        % (
            prefix,
            prefix,
            prefix,
            len(pool.items),
            prefix,
            len(code),
            prefix,
            len(nodes),
            prefix,
            len(variables),
            prefix,
            len(lines),
            prefix,
            len(commands),
            prefix,
            len(funcs),
            h,
        )
    )
    src.append("")
    os.makedirs(args.out, exist_ok=True)
    LF = chr(10)
    with open(
        os.path.join(args.out, "%s_program.c" % prefix), "w", encoding="utf-8", newline=LF
    ) as f:
        f.write(LF.join(src))
    with open(
        os.path.join(args.out, "%s_program.h" % prefix), "w", encoding="utf-8", newline=LF
    ) as f:
        f.write(LF.join(hdr))
    expanded = args.profile == "expanded"
    rom_estimate = (
        pos
        + len(offsets) * (4 if expanded else 2)
        + len(code)
        + len(nodes) * (20 if expanded else 10)
        + len(variables) * (16 if expanded else 12)
        + len(lines) * (8 if expanded else 4)
        + len(commands) * (76 if expanded else 40)
        + len(funcs) * 4
    )
    print(
        "Converted %s: %d nodes, %d lines, %d vars, %d commands, ~%d ROM bytes"
        % (prefix, len(nodes), len(lines), len(variables), len(commands), rom_estimate)
    )


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("project")
    ap.add_argument("--out", required=True)
    ap.add_argument("--prefix", default="dialogue")
    ap.add_argument("--profile", choices=("compact", "expanded"), default="compact")
    ap.add_argument("--lines", help="Official line-table CSV for a precompiled .yarnc file")
    ap.add_argument("--metadata", help="Official metadata CSV for a precompiled .yarnc file")
    ap.add_argument("--definitions", help="Command and function declarations in .ysls.json format")
    ap.add_argument("--max-line-length", type=int, default=200)
    args = ap.parse_args()
    args.max_vars = 4096 if args.profile == "expanded" else 48
    args.max_options = 16 if args.profile == "expanded" else 6
    args.max_substitutions = args.max_parameters = 8 if args.profile == "expanded" else 4
    try:
        if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", args.prefix):
            raise CompileError(
                "The prefix must begin with a letter and contain only letters, digits and underscores."
            )
        if args.max_line_length < 1:
            raise CompileError("The line-length limit must be positive.")
        compile_program(args)
    except (
        CompileError,
        ValueError,
        OSError,
        KeyError,
        struct.error,
        subprocess.TimeoutExpired,
    ) as e:
        print("error:", e, file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
