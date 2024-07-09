#!/usr/bin/env python3
"""A module for reading Bude Binary Word-oriented Format (BudeBWF) files."""

from __future__ import annotations
import argparse
import os
import sys


CURRENT_VERSION_NUMBER = 3



class ParseError(Exception):
    """Exception signalling an error in parsing a BudeBWF file."""


def read_bytecode(filename: str, strict=True) -> tuple[list[str], list[bytes]]:
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
        field_count = 2
        fields_read = 0
        if version_number >= 2:
            field_count = int.from_bytes(f.read(4), "little", signed=True)
            if field_count < 2:
                raise ParseError(f"`data-info-field-count` must be at least 2, not {field_count}")
        string_count = int.from_bytes(f.read(4), "little", signed=True)
        fields_read += 1
        function_count = int.from_bytes(f.read(4), "little", signed=True)
        fields_read += 1
        fields_left = field_count - fields_read
        assert fields_left >= 0
        if fields_left > 0:
            f.read(4 * fields_left)
        strings = []
        functions = []
        for _ in range(string_count):
            length = int.from_bytes(f.read(4), "little")
            strings.append(f.read(length).decode())
        for _ in range(function_count):
            entry_size = None
            bytes_read = 0
            if version_number >= 3:
                entry_size = int.from_bytes(f.read(4), "little")
                bytes_read += 4
            size = int.from_bytes(f.read(4), "little")
            bytes_read += 4
            if entry_size is None:
                entry_size = size
            functions.append(f.read(size))
            bytes_read += size
            diff = entry_size + 4 - bytes_read
            if diff > 0:
                # Skip excess bytes
                f.read(diff)
    return strings, functions


def display_bytecode(strings: list[str], functions: list[bytes], *, file=sys.stdout) -> None:
    """Display the strings and functions in a human-readable format."""
    for i, string in enumerate(strings):
        print(f"str_{i}:\n\t{string!r}", file=file)
    for i, func in enumerate(functions):
        print(f"func_{i}:\n\t{' '.join(f'{b:02x}' for b in func)}", file=file)


def main() -> None:
    arg_parser = argparse.ArgumentParser(description="Display the contents of a BudeBWF file.")
    arg_parser.add_argument("filename", help="the file to be read")
    arg_parser.add_argument("-o", help="the file to write the output to (defaut: stdout)")
    args = arg_parser.parse_args()
    if args.o is None or args.o == "-":
        args.o = sys.stdout
    strings, functions = read_bytecode(args.filename)
    display_bytecode(strings, functions, file=args.o)


if __name__ == "__main__":
    main()
