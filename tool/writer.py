#!/usr/bin/env python3
"""A module for writing Bude Binary Word-oriented Format (BudeBWF) files."""

import argparse
import ast
import sys

import ir


CURRENT_VERSION_NUMBER = 1


def write_header(version_number: int) -> bytes:
    return f"BudeBWFv{version_number}\n".encode()


def write_data_info(module: ir.Module, version_number: int) -> bytes:
    output = bytearray()
    output.extend(len(module.strings).to_bytes(length=4, byteorder="little", signed=True))
    output.extend(len(module.functions).to_bytes(length=4, byteorder="little", signed=True))
    return bytes(output)


def write_data(module: ir.Module, version_number: int) -> bytes:
    output = bytearray()
    for string in module.strings:
        output.extend(len(string).to_bytes(length=4, byteorder="little"))
        output.extend(string.encode())
    for function in module.functions:
        output.extend(len(function.code).to_bytes(length=4, byteorder="little", signed=True))
        output.extend(function.code)
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
