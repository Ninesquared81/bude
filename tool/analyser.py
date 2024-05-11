"""This module analyses the bytecode produced by the Bude compiler."""

from __future__ import annotations

import argparse

import matplotlib.pyplot as plt
import numpy as np

import ir
import reader


def plot_bar(fig: plt.Figure, counts: np.ndarray, opcode_names: list[str]) -> None:
    """Plot a bar chart to show the frequency of each IR instruction."""
    BAR_THICKNESS = 10.
    ax = plt.subplot(2, 1, 1)
    ax.tick_params(axis="x", rotation=90, labelsize="xx-small")
    bar_xs = [op * (BAR_THICKNESS + 1) for op in ir.Opcode]
    ax.bar(bar_xs, counts, width=BAR_THICKNESS, tick_label=opcode_names)


def plot_pie(fig: plt.Figure, counts: np.ndarray, opcode_names: list[str]) -> None:
    """Plot a pie chart to show the frequency of each IR instruction."""
    ax = plt.subplot(2, 1, 2)
    pie_xs, pie_labels = zip(
        *((count, name) for count, name
          in zip(counts, opcode_names) if count > 0)
    )
    ax.pie(pie_xs, labels=pie_labels)


def analyse_bytecode(functions: list[bytes]) -> None:
    """Analyse the frequency of each IR instruction."""
    counts = np.zeros(len(ir.Opcode), dtype=int)
    opcode_names = [op.name for op in ir.Opcode]
    for i, func in enumerate(functions):
        print(f"func_{i}:", *ir.Block(func), sep="\n\t")
        for instruction in ir.Block(func):
            counts[instruction.op] += 1
    fig = plt.figure(figsize=(16., 10.))
    plot_bar(fig, counts, opcode_names)
    plot_pie(fig, counts, opcode_names)
    plt.show()


def main() -> None:
    arg_parser = argparse.ArgumentParser(description="Analyse a BudeBWF file")
    arg_parser.add_argument("filename", help="the file to analyse")
    args = arg_parser.parse_args()
    try:
        strings, functions = reader.read_bytecode(args.filename)
    except reader.ParseError as e:
        print(e)
        exit(1)
    analyse_bytecode(functions)


if __name__ == "__main__":
    main()
