#!/usr/bin/env python3
"""A module for writing Bude Binary Word-oriented Format (BudeBWF) files."""

import argparse
import ast
import itertools
import sys
from typing import BinaryIO

import ir


CURRENT_VERSION_NUMBER = 4


def get_field_count(version_number: int) -> int:
    field_counts = {
        1: 2,
        2: 2,
        3: 2,
        4: 3,
    }
    return field_counts[version_number]


def write_s32(f: BinaryIO, n: int) -> int:
    return f.write(n.to_bytes(4, "little", signed=True))


def write_u32(f: BinaryIO, n: int) -> int:
    return f.write(n.to_bytes(4, "little"))


def write_header(f: BinaryIO, version_number: int) -> None:
    f.write(f"BudeBWFv{version_number}\n".encode())


def write_data_info(f: BinaryIO, module: ir.Module, version_number: int) -> None:
    field_count = get_field_count(version_number)
    bytes_written = 0
    if version_number >= 2:
        bytes_written += write_s32(f, field_count)
    bytes_written += write_s32(f, len(module.strings))
    bytes_written += write_s32(f, len(module.functions))
    if version_number >= 4:
        bytes_written += write_s32(f, len(module.user_defined_types))
    assert bytes_written == 4 + field_count*4, f"{bytes_written = }, {field_count*4 = }"


def write_function(f: BinaryIO, function: ir.Function, version_number: int) -> None:
    size = function.code.size
    local_count = len(function.locals)
    bytes_written = 0
    if version_number >= 3:
        entry_size = 4 + size
        if version_number >= 4:
            entry_size += 3*4 + local_count*4
        bytes_written += write_s32(f, entry_size)
    else:
        entry_size = size
    bytes_written += write_s32(f, size)
    bytes_written += f.write(function.code.code)
    if version_number >= 4:
        bytes_written += write_s32(f, function.max_for_loop_level)
        bytes_written += write_s32(f, function.locals_size)
        bytes_written += write_s32(f, local_count)
        for local in function.locals:
            bytes_written += write_s32(f, local)
    assert bytes_written == entry_size + 4, f"{bytes_written = }, {entry_size + 4 = }"


def write_ud_type(f: BinaryIO, ud_type: ir.UserDefinedType, version_number: int) -> None:
    bytes_written = 0
    field_count = len(ud_type.fields)
    entry_size = 3*4 + field_count*4
    bytes_written += write_s32(f, entry_size)
    bytes_written += write_s32(f, ud_type.kind)
    bytes_written += write_s32(f, field_count)
    bytes_written += write_s32(f, ud_type.word_count)
    for field in ud_type.fields:
        bytes_written += write_s32(f, field)
    assert bytes_written == entry_size + 4, f"{bytes_written = }, {entry_size + 4 = }"


def write_data(f: BinaryIO, module: ir.Module, version_number: int) -> None:
    for string in module.strings:
        write_u32(f, len(string))
        f.write(string.encode())
    for function in module.functions:
        write_function(f, function, version_number)
    if version_number < 4:
        return
    for ud_type in module.user_defined_types:
        write_ud_type(f, ud_type, version_number)

def write_bytecode(filename: str, module: ir.Module,
                   version_number: int = CURRENT_VERSION_NUMBER) -> None:
    """Write Bude bytecode to a BudeBWF file."""
    with open(filename, "wb") as f:
        write_header(f, version_number)
        write_data_info(f, module, version_number)
        write_data(f, module, version_number)


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
        raise ValueError(f"Incorrect number of operands -- expected {n_types} but got {n_real}.")
    return ir.Instruction(op, *[t(operand) for t, operand in zip(operand_types, operands)])


def parse_beech(src: str) -> tuple[dict|list|str, str]:
    src = src.strip()
    if src.startswith("{"):
        src = src.removeprefix("{")
        d = {}
        while src and src[0] != "}":
            try:
                key, rest = src.split(maxsplit=1)
            except ValueError:
                raise ValueError("Expected key-value pair")
            value, src = parse_beech(rest)
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


def parse_new(args: str, module_builder: ir.ModuleBuilder) -> None:
    args = args.strip()
    match args.split(maxsplit=1):
        case ["string", literal]:
            try:
                string = ast.literal_eval(literal)
            except (ValueError, SyntaxError) as e:
                print(f"Failed to parse string: {e}", file=sys.stderr)
            else:
                idx = module_builder.add_string(string)
                print(f"New string created: {idx}.", file=sys.stderr)
        case ["function", *_]:
            idx = module_builder.new_function()
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
                print(f"Unknown key {e}")
            else:
                idx = module_builder.add_type(ud_type)
                print(f"New type created: {idx+ir.BUILTIN_TYPE_COUNT}", file=sys.stderr)
        case ["string"]:
            print(f"No string literal provided", file=sys.stderr)
        case [other, *_]:
            print(f"Unknown target {other!r}", file=sys.stderr)


def main() -> None:
    arg_parser = argparse.ArgumentParser(description="Create a BudeBWF file.")
    arg_parser.add_argument("filename", help="The file to save into")
    arg_parser.add_argument("--verbose", help="If enabled, print extra info", action="store_true")
    args = arg_parser.parse_args()
    module_builder = ir.ModuleBuilder()
    while True:
        inp = input("> ").strip()
        if inp.startswith("("):
            try:
                instruction = parse_instruction(inp[1:])
            except ValueError as e:
                print(f"{e}. Instruction not processed", file=sys.stderr)
            else:
                module_builder.add_instruction(instruction)
        elif inp.lower().startswith("end"):
            break
        elif inp.lower().startswith("new"):
            parse_new(inp[3:], module_builder)
        else:
            print(f"Unknown input '{inp}'", file=sys.stderr)
    module = module_builder.build()
    if args.verbose:
        module.pprint()
    write_bytecode(args.filename, module)


if __name__ == "__main__":
    main()
