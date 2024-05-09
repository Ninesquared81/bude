"""This module analyses the bytecode produced by the Bude compiler."""

import argparse

import reader


def main() -> None:
    arg_parser = argparse.ArgumentParser(description="Analyse a BudeBWF file")
    arg_parser.add_argument("filename", help="the file to analyse")
    args = arg_parser.parse_args()
    try:
        strings, functions = reader.read_bytecode(args.filename)
    except reader.ParseError as e:
        print(e)
        exit(1)
    reader.display_bytecode(strings, functions)


if __name__ == "__main__":
    main()
