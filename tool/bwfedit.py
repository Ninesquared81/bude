import argparse
import ast
import itertools
import sys
from typing import Self

import ir
import reader
import writer


def parse_beech(src: str) -> tuple[dict|list|str, str]:
    src = src.strip()
    if src.startswith("{"):
        src = src.removeprefix("{")
        d = {}
        while src and src[0] != "}":
            src = src.lstrip()
            try:
                if src.startswith("'") or src.startswith('"'):
                    key, src = parse_beech(src)
                else:
                    key, src = src.split(maxsplit=1)
            except ValueError:
                raise ValueError("Expected key-value pair")
            value, src = parse_beech(src)
            d[key] = value
        if not src:
            raise ValueError("Unterminated tree")
        return d, src.removeprefix("}")
    if src.startswith("("):
        src = src.removeprefix("(")
        lst = []
        while src and src[0] != ")":
            value, src = parse_beech(src)
            lst.append(value)
        if not src:
            raise ValueError("Unterminated list")
        return lst, src.removeprefix(")")
    if src.startswith("'") or src.startswith('"'):
        delim = src[0]
        start = 1
        while True:
            end = src.find(delim, start)
            if end == -1:
                raise ValueError("Unterminated string")
            if src[end-1] != "\\":
                break
            start = end + 1
        string = ast.literal_eval(src[:end+1])
        return string, src[end+1:]
    try:
        value, *rest = src.split(maxsplit=1)
    except ValueError:
        raise ValueError("Expected value")
    brackets = []
    while value.endswith("}") or value.endswith(")"):
        brackets.append(value[-1])
        value = value[:-1]
    src = "".join(itertools.chain(reversed(brackets), rest))
    return value, src


class Editor:
    """A simple BudeBWF editor."""

    def __init__(self, builder: ir.ModuleBuilder | None = None) -> None:
        if builder is None:
            builder = ir.ModuleBuilder()
        self.builder = builder

    @classmethod
    def from_file(cls, filename: str) -> Self:
        with open(filename, "rb") as f:
            module = reader.read_bytecode(f)
        return cls.from_module(module)

    @classmethod
    def from_module(cls, module: ir.ModuleBuilder) -> Self:
        return cls(ir.ModuleBuilder.from_module(module))

    def save_file(self, filename: str) -> None:
        module = self.finish()
        with open(filename, "wb") as f:
            writer.write_bytecode(f, module)

    def finish(self) -> ir.Module:
        return self.builder.build()

    def run(self) -> None:
        """Main editor driver function."""
        while True:
            inp = input("> ").strip()
            if inp.startswith("("):
                try:
                    instruction = self.parse_instruction(inp[1:])
                except ValueError as e:
                    print(f"{e}. Instruction not processed", file=sys.stderr)
                else:
                    self.builder.add_instruction(instruction)
            elif inp.lower().startswith("end"):
                break
            elif inp.lower().startswith("new"):
                self.parse_new(inp[3:])
            elif inp.lower().startswith("show"):
                self.parse_show(inp[4:])
            elif inp.lower().startswith("insert"):
                self.parse_insert(inp[6:])
            elif inp.lower().startswith("amend"):
                self.parse_amend(inp[5:])
            else:
                print(f"Unknown input '{inp}'", file=sys.stderr)

    def display(self) -> None:
        """Display the current state of the module."""
        module = self.builder.build()
        module.pprint()

    @staticmethod
    def parse_instruction(src: str) -> ir.Instruction:
        src = src.strip()
        if not src.endswith(")"):
            raise ValueError("Instruction must be enclosed in brackets '(', ')'")
        op_string, *operands = src[:-1].strip().split()
        try:
            op = ir.Opcode[op_string]
        except KeyError:
            raise ValueError(f"Invalid opcode {op_string}")
        operand_types = ir.Block.INSTRUCTION_TABLE[op]
        if (n_real := len(operands)) != (n_types := len(operand_types)):
            raise ValueError(f"Incorrect number of operands -- expected {n_types}"\
                             f"but got {n_real}.")
        return ir.Instruction(op, *[t(operand) for t, operand in zip(operand_types, operands)])


    def parse_new(self, args: str) -> None:
        args = args.strip()
        match args.split(maxsplit=1):
            case ["string", literal]:
                try:
                    string = ast.literal_eval(literal)
                except (ValueError, SyntaxError) as e:
                    print(f"Failed to parse string: {e}", file=sys.stderr)
                else:
                    idx = self.builder.add_string(string)
                    print(f"New string created: {idx}.", file=sys.stderr)
            case ["function", *_]:
                idx = self.builder.new_function()
                print(f"New function created: {idx}.", file=sys.stderr)
            case ["type", rest]:
                rest = rest.strip()
                if not (rest.startswith("{") and rest.endswith("}")):
                    print(f"Type must be enclosed in '{'...'}'", file=sys.stderr)
                    return
                try:
                    type_dict, _ = parse_beech(rest)
                except ValueError as e:
                    print(f"Failed to parse type: {e}")
                    return
                assert(isinstance(type_dict, dict))
                try:
                    ud_type = ir.UserDefinedType(
                        kind=ir.TypeKind[type_dict["kind"].upper()],
                        word_count=int(type_dict["word_count"]),
                        fields=[int(field) for field in type_dict["fields"]]
                    )
                except ValueError as e:
                    print(f"Failed to parse type: {e}", file=sys.stderr)
                except KeyError as e:
                    print(f"Unknown key {e}", file=sys.stderr)
                else:
                    idx = self.builder.add_type(ud_type)
                    print(f"New type created: {idx+ir.BUILTIN_TYPE_COUNT}", file=sys.stderr)
            case ["external", rest]:
                rest = rest.strip()
                if not (rest.startswith("{") and rest.endswith("}")):
                    print("External function must be enclosed in '{'...'}'", file=sys.stderr)
                    return
                try:
                    ext_dict, _ = parse_beech(rest)
                except ValueError as e:
                    print(f"Failed to parse external function: {e}")
                    return
                assert(isinstance(ext_dict, dict))
                try:
                    name = ext_dict["name"]
                    params = [int(t) for t in ext_dict["sig"]["params"]]
                    rets = [int(t) for t in ext_dict["sig"]["rets"]]
                    external = ir.ExternalFunction(
                        sig=ir.Signature(params, rets),
                        name=name,
                        call_conv=ir.CallingConvention[ext_dict["call-conv"]]
                    )
                except ValueError as e:
                    print(f"Failed to parse external function: {e}", file=sys.stderr)
                except KeyError as e:
                    print(f"Unknown key {e}", file=sys.stderr)
                else:
                    idx = self.builder.add_external(external)
                    str_idx = self.builder.add_string(name)
                    print(f"New external function created: {idx}", file=sys.stderr)
                    print(f"New string created: {str_idx}", file=sys.stderr)
            case ["ext_library", literal]:
                try:
                    filename = ast.literal_eval(literal)
                except (ValueError, SyntaxError) as e:
                    print(f"Failed to parse filename: {e}", file=sys.stderr)
                else:
                    idx = self.builder.new_ext_library(filename)
                    str_idx = self.builder.add_string(filename)
                    print(f"New external library created: {idx}", file=sys.stderr)
                    print(f"New string created: {str_idx}", file=sys.stderr)
            case ["string"]:
                print(f"No string literal provided", file=sys.stderr)
            case [other, *_]:
                print(f"Unknown target {other!r}", file=sys.stderr)

    def parse_show(self, src: str) -> None:
        src = src.strip()
        match src.split(maxsplit=1):
            case []:
                self.display()
            case ["i"]:
                accum = 0
                for i, ins in enumerate(self.builder.instructions):
                    print(f"{i: >3} [acc.{accum: >3}]: {ins}")
                    accum += ins.size()
                print(f"[total size {accum}]")
            case _:
                print(f"Unknown show target {src!r}")

    def parse_insert(self, src: str) -> None:
        src = src.strip()
        try:
            idx, rest = src.split(maxsplit=1)
        except ValueError:
            print(f"Unknown insert target {src!r}", file=stderr)
            return
        try:
            idx = int(idx)
        except ValueError:
            print(f"({e}", file=sys.stderr)
            return
        src = rest.strip()
        if not src.startswith("("):
            print("Instruction must start with '('", file=stderr)
            return
        try:
            instruction = self.parse_instruction(src.removeprefix("("))
        except ValueError as e:
            print(f"{e}. Instruction not processed.", file=sys.stderr)
        else:
            self.builder.insert_instruction(idx, instruction)

    def parse_amend(self, src: str) -> None:
        src = src.strip()
        try:
            idx, rest = src.split(maxsplit=1)
        except ValueError:
            print(f"Unknown insert target {src!r}", file=sys.stderr)
            return
        try:
            idx = int(idx)
        except ValueError:
            print(f"({e}", file=sys.stderr)
            return
        src = rest.strip()
        if not src.startswith("("):
            print("Instruction must start with '('", file=sys.stderr)
            return
        try:
            instruction = self.parse_instruction(src.removeprefix("("))
        except ValueError as e:
            print(f"{e}. Instruction not processed.", file=sys.stderr)
        else:
            self.builder.instructions[idx] = instruction


def main() -> None:
    arg_parser = argparse.ArgumentParser(description="Edit a BudeBWF file.")
    arg_parser.add_argument("filename", help="The file to edit")
    args = arg_parser.parse_args()
    try:
        editor = Editor.from_file(args.filename)
    except FileNotFoundError:
        editor = Editor()  # Create a new, blank module.
    editor.run()
    editor.save_file(args.filename)



if __name__ == "__main__":
    main()
