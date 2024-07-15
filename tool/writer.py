#!/usr/bin/env python3
"""A module for writing Bude Binary Word-oriented Format (BudeBWF) files."""

import argparse
import ast
import sys

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


def write_header(version_number: int) -> bytes:
    return f"BudeBWFv{version_number}\n".encode()


def write_data_info(module: ir.Module, version_number: int) -> bytes:
    output = bytearray()
    field_count = get_field_count(version_number)
    if version_number >= 2:
        output.extend(field_count.to_bytes(4, "little", signed=True))
    output.extend(len(module.strings).to_bytes(4, "little", signed=True))
    output.extend(len(module.functions).to_bytes(4, "little", signed=True))
    if version_number >= 4:
        output.extend(len(module.user_defined_types).to_bytes(4, "little", signed=True))
    return bytes(output)


def write_data(module: ir.Module, version_number: int) -> bytes:
    output = bytearray()
    for string in module.strings:
        output.extend(len(string).to_bytes(4, "little"))
        output.extend(string.encode())
    for function in module.functions:
        size = function.code.size
        local_count = len(function.locals)
        if version_number >= 3:
            entry_size = 4 + size
            if version_number >= 4:
                entry_size += local_count * 4
            output.extend(entry_size.to_bytes(4, "little", signed=True))
        else:
            entry_size = size
        output.extend(size.to_bytes(4, "little", signed=True))
        output.extend(function.code.code)
        if version_number >= 4:
            output.extend(function.max_for_loop_level.to_bytes(4, "little", signed=True))
            output.extend(function.locals_size.to_bytes(4, "little", signed=True))
            output.extend(local_count.to_bytes(4, "little", signed=True))
            for local in function.locals:
                output.extend(local.to_bytes(4, "little", signed=True))
    if version_number < 4:
        return bytes(output)
    for ud_type in module.user_defined_types:
        field_count = len(ud_type.fields)
        entry_size = 3*4 + field_count*4
        output.extend(entry_size.to_bytes(4, "little", signed=True))
        output.extend(ud_type.kind.to_bytes(4, "little", signed=True))
        output.extend(field_count.to_bytes(4, "little", signed=True))
        output.extend(ud_type.word_count.to_bytes(4, "little", signed=True))
        for field in ud_type.fields:
            output.extend(ud.to_bytes(4, "little", signed=True))
    return bytes(output)

def write_bytecode(module: ir.Module, version_number: int = CURRENT_VERSION_NUMBER) -> bytes:
    """Write Bude bytecode to a BudeBWF file."""
    output = bytearray()
    output += write_header(version_number)
    output += write_data_info(module, version_number)
    output += write_data(module, version_number)
    return bytes(output)


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


def parse_new(args: str, module_builder: ir.ModuleBuilder) -> None:
    args = args.strip()
    match args.split(maxsplit=1):
        case ["string", literal]:
            idx = module_builder.add_string(ast.literal_eval(literal))
            print(f"New string created: {idx}.")
        case ["function"]:
            idx = module_builder.new_function()
            print(f"New function created: {idx}.")


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
    output = write_bytecode(module)
    with open(args.filename, "wb") as f:
        f.write(output)

if __name__ == "__main__":
    main()
