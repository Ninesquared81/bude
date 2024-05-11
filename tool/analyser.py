"""This module analyses the bytecode produced by the Bude compiler."""

import argparse

import matplotlib.pyplot as plt
import numpy as np

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
    counts = np.zeros(len(ir.Opcode), dtype=int)
    opcode_names = [op.name for op in ir.Opcode]
    for i, func in enumerate(functions):
        print(f"func_{i}:", *ir.Block(func), sep="\n\t")
        for instruction in ir.Block(func):
            counts[instruction.op] += 1
    # print(*(f"{opcode}: {count}" for opcode, count in zip(opcode_names, counts)), sep="\n")
    BAR_THICKNESS = 10.
    plt.figure(figsize=(16., 6.4))
    plt.xticks(rotation=90, fontsize="xx-small")
    bar_xs = [op * (BAR_THICKNESS + 1) for op in ir.Opcode]
    plt.bar(bar_xs, counts, width=BAR_THICKNESS, tick_label=opcode_names)
    plt.show()

if __name__ == "__main__":
    main()
