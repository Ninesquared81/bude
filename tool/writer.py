#!/usr/bin/env python3
"""A module for writing Bude Binary Word-oriented Format (BudeBWF) files."""


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
