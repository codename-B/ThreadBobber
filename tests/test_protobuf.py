"""Exercise the protobuf boundary without requiring the Yarn compiler."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import threadbobber as converter
import yarn_spinner_pb2 as yarn


class ProtobufTests(unittest.TestCase):
    def program(self, instruction=None):
        program = yarn.Program(name="test", language_version=4)
        node = program.nodes["Start"]
        node.name = "Start"
        if instruction is not None:
            node.instructions.add().CopyFrom(instruction)
        return program

    def decode(self, program):
        return converter.decode_program(program.SerializeToString())

    def test_defaults_and_oneof_presence(self):
        program = self.program(yarn.Instruction(pushBool={"value": False}))
        program.initial_values["$bool"].bool_value = False
        program.initial_values["$number"].float_value = 0
        program.initial_values["$text"].string_value = ""
        result = self.decode(program)
        self.assertEqual(result["nodes"][0]["instructions"], [("pushBool", {"value": False})])
        self.assertEqual(result["initial"], {
            "$bool": ("bool", False), "$number": ("number", 0.0), "$text": ("string", "")
        })

    def test_upstream_operand_names_and_scalar_defaults(self):
        instruction = yarn.Instruction(addOption={"lineID": "line:hello", "destination": -1})
        result = self.decode(self.program(instruction))
        self.assertEqual(result["nodes"][0]["instructions"], [("addOption", {
            "line_id": "line:hello", "destination": -1,
            "substitution_count": 0, "has_condition": False,
        })])

    def test_return_keyword_and_empty_operation(self):
        instruction = yarn.Instruction()
        getattr(instruction, "return").SetInParent()
        result = self.decode(self.program(instruction))
        self.assertEqual(result["nodes"][0]["instructions"], [("return", {})])

    def test_unknown_instruction_rejected_alone_or_with_known_operation(self):
        # Future operation field 24, length-delimited empty message.
        for blob in (b"\xc2\x01\x00", b"\x7a\x00\xc2\x01\x00"):
            with self.subTest(blob=blob):
                instruction = yarn.Instruction.FromString(blob)
                with self.assertRaisesRegex(converter.CompileError, "Unsupported Instruction"):
                    self.decode(self.program(instruction))

    def test_unknown_operand_rejected(self):
        instruction = yarn.Instruction(pushBool={})
        instruction.pushBool.ParseFromString(b"\x10\x01")
        with self.assertRaisesRegex(converter.CompileError, "Unsupported operand"):
            self.decode(self.program(instruction))

    def test_missing_instruction_or_initial_value_rejected(self):
        with self.assertRaisesRegex(converter.CompileError, "Instruction must hold"):
            self.decode(self.program(yarn.Instruction()))
        program = self.program()
        program.initial_values["$unset"].SetInParent()
        with self.assertRaisesRegex(converter.CompileError, "Operand must hold"):
            self.decode(program)

    def test_unknown_initial_value_rejected(self):
        program = self.program()
        program.initial_values["$future"].ParseFromString(b"\x20\x01")
        with self.assertRaisesRegex(converter.CompileError, "Unsupported Operand"):
            self.decode(program)

    def test_truncated_program_reports_compile_error(self):
        with self.assertRaisesRegex(converter.CompileError, "Invalid Yarn Spinner protobuf"):
            converter.decode_program(b"\x12\x05\x00")

    def test_unknown_program_metadata_is_ignored(self):
        program = self.program()
        self.assertEqual(
            converter.decode_program(program.SerializeToString() + b"\xa0\x06\x01"),
            self.decode(program),
        )

    def test_node_order_is_independent_of_map_serialization(self):
        first = self.program()
        first.nodes["Z"].name = "Z"
        second = self.program()
        second.nodes["A"].name = "A"
        # Merging serialized maps in opposite orders must give identical output.
        a, b = first.SerializeToString(), second.SerializeToString()
        result = converter.decode_program(a + b)
        self.assertEqual(result, converter.decode_program(b + a))
        self.assertEqual([node["name"] for node in result["nodes"]], ["A", "Start", "Z"])


if __name__ == "__main__":
    unittest.main()
