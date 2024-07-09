#!/usr/bin/env python3
"""A module for reading Bude Binary Word-oriented Format (BudeBWF) files."""

from __future__ import annotations
import argparse
import sys


CURRENT_VERSION_NUMBER = 2



class ParseError(Exception):
    """Exception signalling an error in parsing a BudeBWF file."""


def read_bytecode(filename: str) -> tuple[list[str], list[bytes]]:
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
            raise ParseError(f"Unsupported BudeBWF version: {version_number}")
        field_count = 2
        if version_number >= 2:
            field_count = int.from_bytes(f.read(4), "little", signed=True)
            if field_count < 2:
                raise ParseError(f"`data-info-field-count` must be at least 2, not {field_count}")
        string_count = int.from_bytes(f.read(4), "little", signed=True)
        function_count = int.from_bytes(f.read(4), "little", signed=True)
        strings = []
        functions = []
        for _ in range(string_count):
            length = int.from_bytes(f.read(4), "little")
            strings.append(f.read(length).decode())
        for _ in range(function_count):
            size = int.from_bytes(f.read(4), "little")
            functions.append(f.read(size))
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
