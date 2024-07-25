#!/usr/bin/env python3
"""A module for writing Bude Binary Word-oriented Format (BudeBWF) files."""

import argparse
import ast
from typing import BinaryIO

import ir


CURRENT_VERSION_NUMBER = 5


def get_field_count(version_number: int) -> int:
    field_counts = {
        1: 2,
        2: 2,
        3: 2,
        4: 3,
        5: 5,
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
    if version_number >= 5:
        bytes_written += write_s32(f, len(module.externals))
        bytes_written += write_s32(f, len(module.ext_libraries))
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


def write_ext_function(f: BinaryIO, external: ir.ExternalFunction, strings: list[str],
                       version_number: int) -> None:
    bytes_written = 0
    param_count = len(external.sig.params)
    ret_count = len(external.sig.rets)
    entry_size = 2*4 + param_count*4 + ret_count*4 + 2*4
    bytes_written += write_s32(f, entry_size)
    bytes_written += write_s32(f, param_count)
    bytes_written += write_s32(f, ret_count)
    for param in external.sig.params:
        bytes_written += write_s32(f, param)
    for ret in external.sig.rets:
        bytes_written += write_s32(f, ret)
    bytes_written += write_s32(f, strings.index(external.name))
    bytes_written += write_s32(f, external.call_conv)
    assert bytes_written == entry_size + 4, f"{bytes_written = }, {entry_size + 4 = }"


def write_ext_library(f: BinaryIO, library: ir.ExternalLibrary, strings: list[str],
                      version_number: int) -> None:
    bytes_written = 0
    external_count = len(library.indices)
    entry_size = 4 + external_count*4 + 4
    bytes_written += write_s32(f, entry_size)
    bytes_written += write_s32(f, external_count)
    for index in library.indices:
        bytes_written += write_s32(f, index)
    bytes_written += write_s32(f, strings.index(library.filename))
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
    if version_number < 5:
        return
    for external in module.externals:
        write_ext_function(f, external, module.strings, version_number)
    for library in module.ext_libraries:
        write_ext_library(f, library, module.strings, version_number)


def write_bytecode(f: BinaryIO, module: ir.Module,
                   version_number: int = CURRENT_VERSION_NUMBER) -> None:
    """Write Bude bytecode to a BudeBWF file."""
    write_header(f, version_number)
    write_data_info(f, module, version_number)
    write_data(f, module, version_number)



def main() -> None:
    arg_parser = argparse.ArgumentParser(description="Create a BudeBWF file.")
    arg_parser.add_argument("filename", help="The file to save into")
    arg_parser.add_argument("--verbose", help="If enabled, print extra info", action="store_true")
    args = arg_parser.parse_args()
    module_builder = ir.ModuleBuilder()
    module = module_builder.build()
    if args.verbose:
        module.pprint()
    with open(args.filename, "wb") as f:
        write_bytecode(f, module)


if __name__ == "__main__":
    main()
