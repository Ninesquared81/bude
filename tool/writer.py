#!/usr/bin/env python3
"""A module for writing Bude Binary Word-oriented Format (BudeBWF) files."""

import argparse
import sys

import ir


CURRENT_VERSION_NUMBER = 1


def write_header(output: bytearray, version_number: int) -> None:
    output.extend(f"BudeBWFv{version_number}\n".encode())


def write_data_info(output: bytearray, module: ir.Module, version_number: int) -> None:
    output.extend(len(module.strings).to_bytes(length=4, byteorder="little", signed=True))
    output.extend(len(module.functions).to_bytes(length=4, byteorder="little", signed=True))


def write_data(output: bytearray, module: ir.Module, version_number: int) -> None:
    for string in strings:
        output.extend(len(string).to_bytes(length=4, byteorder="little"))
        output.extend(string.encode())
    for function in functions:
        output.extend(len(function.code).to_bytes(length=4, byteorder="little", signed=True))
        output.extend(function.code)


def write_bytecode(module: ir.Module, filename: str,
                   version_number: int = CURRENT_VERSION_NUMBER) -> None:
    """Write Bude bytecode to a BudeBWF file."""
    output = bytearray()
    write_header(output, version_number)
    write_data_info(output, module, version_number)
    write_data(ouput, module, version_number)
    with open(filename, "wb") as f:
        f.write(output)


def main() -> None:
    arg_parser = argparse.ArgumentParser(description="Create a BudeBWF file.")
    arg_parser.add_argument("filename", help="The file to save into")
    program = bytearray()
    while (inp := input("> ")) != "end":
        if not (inp.startswith("(") and inp.endswith(")")):
            print("Error, instruction must be encloded in brackets '(', ')'", file=sys.stdout)
            continue
        op_string, *operands = inp[1:len(inp)-1].strip().split()
        try:
            op = ir.Opcode[op_string]
        except KeyError:
            print(f"Invalid opcode {op_string}. Instruction not processed.", file=sys.stderr)
            continue
        operand_types = ir.Block.INSTRUCTION_TABLE[op]
        if (n_real := len(operands)) != (n_types := len(operand_types)):
            print(f"Incorrect number of operands -- expected {n_types} but got {n_real}.",
                  "Instruction not processed.",
                  file=sys.stderr)
            continue
        instruction = ir.Instruction(op, *[t(operand) for t, operand in
                                            zip(operand_types, operands)])
        program.extend(ir.Block.encode(instruction))
    print(*ir.Block(program))


if __name__ == "__main__":
    main()
