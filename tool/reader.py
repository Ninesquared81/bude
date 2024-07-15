#!/usr/bin/env python3
"""A module for reading Bude Binary Word-oriented Format (BudeBWF) files."""

from __future__ import annotations
import argparse
import dataclasses
import os
import sys
from typing import BinaryIO

import ir


CURRENT_VERSION_NUMBER = 4



class ParseError(Exception):
    """Exception signalling an error in parsing a BudeBWF file."""


@dataclasses.dataclass
class DataInfo:
    """Dataclass holding metadata from the `DATA-INFO` section of a BudeBWF file."""

    string_count: int
    function_count: int
    user_defined_type_count: int


def read_s32(f: BinaryIO, bytes_read: int = 0) -> tuple[int, int]:
    """Read a signed 32-bit (little-endian) integer from a file."""
    return  int.from_bytes(f.read(4), "little", signed=True), bytes_read + 4

def read_u32(f: BinaryIO, bytes_read: int = 0) -> tuple[int, int]:
    """Read an unsigned 32-bit (little-endian) integer from a file."""
    return int.from_bytes(f.read(4), "little"), bytes_read + 4

def read_bytes(f: BinaryIO, size: int, bytes_read: int = 0) -> tuple[bytes, int]:
    """Read an array of bytes from a file."""
    return f.read(size), bytes_read + size

def read_data_info(f: BinaryIO, version_number: int) -> DataInfo:
    """Read the `DATA-INFO` section of a BudeBWF file."""
    field_count = 2
    bytes_read = 0
    if version_number >= 2:
        field_count, _ = read_s32(f)
        if field_count < 2:
            raise ParseError(f"`data-info-field-count` must be at least 2, not {field_count}")
    string_count, bytes_read = read_s32(f, bytes_read)
    function_count, bytes_read = read_s32(f, bytes_read)
    ud_type_count = 0
    if version_number >= 4:
        ud_type_count, bytes_read = read_s32(f, bytes_read)
    bytes_left = field_count*4 - bytes_read
    assert bytes_left >= 0
    if bytes_left > 0:
        f.read(bytes_left)
    return DataInfo(string_count, function_count, ud_type_count)


def read_function(f: BinaryIO, version_number: int) -> ir.Function:
    """Read an entry from the `FUNCTION-TABLE` section of a BudeBWF file."""
    entry_size = None
    bytes_read = 0
    max_for_loop_level = 0
    locals_size = 0
    local_count = 0
    locals_ = []
    if version_number >= 3:
        entry_size, bytes_read = read_s32(f, bytes_read)
        size, bytes_read = read_s32(f, bytes_read)
    if entry_size is None:
        entry_size = size
    code, bytes_read = read_bytes(f, size, bytes_read)
    if version_number >= 4:
        max_for_loop_level, bytes_read = read_s32(f, bytes_read)
        locals_size, bytes_read = read_s32(f, bytes_read)
        local_count, bytes_read = read_s32(f, bytes_read)
        for _ in range(local_count):
            local, bytes_read = read_s32(f, bytes_read)
            locals_.append(local)
    diff = entry_size + 4 - bytes_read
    if diff > 0:
        # Skip excess bytes
        f.read(diff)
    return ir.Function(ir.Block(code), max_for_loop_level, locals_size, locals_)


def read_ud_type(f: BinaryIO, version_number: int) -> ir.UserDefinedType:
    assert version_number >= 4
    entry_size, _ = read_s32(f)
    bytes_read = 0  # Doesn't include `entry-size` field.
    kind, bytes_read = read_s32(f, bytes_read)
    field_count, bytes_read = read_s32(f, bytes_read)
    word_count, bytes_read = read_s32(f, bytes_read)
    fields = []
    for _ in range(field_count):
        field_type, bytes_read = read_s32(f, bytes_read)
        fields.append(field_type)
    bytes_left = entry_size - bytes_read
    assert bytes_left >= 0
    if bytes_left > 0:
        f.read(bytes_left)
    return ir.UserDefinedType(ir.TypeKind(kind), word_count, fields)


def read_bytecode(filename: str, strict=True) -> ir.Module:
    """Read bytecode in file and return a list of strings and functions."""
    with open(filename, "rb") as f:
        header_line = f.readline().decode()
        magic_number, _, version_number_string = header_line.partition("v")
        if magic_number != "BudeBWF":
            raise ParseError("Invalid file")
        try:
            version_number = int(version_number_string.rstrip())
        except ValueError:
            raise ParseError(f"Invalid version number: {version_number_string!r}")
        if version_number <= 0:
            raise ParseError(f"Invalid version number: {version_number}")
        if version_number > CURRENT_VERSION_NUMBER:
            if strict or CURRENT_VERSION_NUMBER < 2:
                raise ParseError(f"Unsupported BudeBWF version: {version_number}")
            else:
                print(f"Warning: version {version_number} is not supported.",
                      "Some data may not be read correctly and some may not be read at all.")
        di = read_data_info(f, version_number)
        strings = []
        functions = []
        user_defined_types = []
        for _ in range(di.string_count):
            length, _ = read_u32(f)
            strings.append(f.read(length).decode())
        for _ in range(di.function_count):
            function = read_function(f, version_number)
            functions.append(function)
        for _ in range(di.user_defined_type_count):
            ud_type = read_ud_type(f, version_number)
            user_defined_types.append(ud_type)
    return ir.Module(strings, functions, user_defined_types)


def main() -> None:
    arg_parser = argparse.ArgumentParser(description="Display the contents of a BudeBWF file.")
    arg_parser.add_argument("filename", help="the file to be read")
    arg_parser.add_argument("-o", help="the file to write the output to (defaut: stdout)")
    args = arg_parser.parse_args()
    if args.o is None or args.o == "-":
        args.o = sys.stdout
    module = read_bytecode(args.filename)
    module.pprint()


if __name__ == "__main__":
    main()
