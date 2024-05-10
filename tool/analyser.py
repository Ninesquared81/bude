"""This module analyses the bytecode produced by the Bude compiler."""

import argparse
import collections

import ir
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
    weights: dict[Opcode, int] = collections.defaultdict(int)
    for i, func in enumerate(functions):
        print(f"func_{i}:", *ir.Block(func), sep="\n\t")
        for instruction in ir.Block(func):
            weights[instruction.op] += 1
    print(weights)


if __name__ == "__main__":
    main()
