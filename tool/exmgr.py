#!/usr/bin/env python3
"""Example manager -- build, run, etc., all the Bude examples in the 'examples' directory."""

import argparse
import pathlib
import subprocess


EXAMPLE_DIR = pathlib.Path("../examples/").resolve()
BUDE_SOURCES = EXAMPLE_DIR.glob("*.bude")
BUDE_EXE = pathlib.Path("../bin/bude.exe").resolve()


def compile_handler(args: argparse.Namespace):
    error_level = 0
    for filename in BUDE_SOURCES:
        proc = subprocess.run([str(BUDE_EXE), str(filename), "-d"],
                              capture_output=True)
        exit_status = proc.returncode
        error_level += abs(exit_status)
        if exit_status == 0:
            print(f"File {filename.name!r} compiled successfully ({exit_status}):")
            if args.dump and not args.quiet:
                print(proc.stdout.decode())
        else:
            print(f"File {filename.name!r} failed to compile ({exit_status}):",
                  proc.stderr.decode(), sep="\n")

    if error_level == 0:
        print("Whole suite compiled successfully!")
    else:
        print("Suite compiled with errors.")
        exit(1)


def main():
    arg_parser = argparse.ArgumentParser(description=__doc__)
    subparsers = arg_parser.add_subparsers(title="subcommands", required=True)
    compile_parser = subparsers.add_parser(
        "compile", help="compile files to word-oriented IR code"
    )
    compile_parser.add_argument(
        "-d", "--dump", action="store_true",
        help="dump IR code to stdout"
    )
    compile_parser.add_argument(
        "-q", "--quiet", action="store_true",
        help="if -d is specified, only emit errors"
    )
    compile_parser.add_argument(
        "-n", "--no-write", action="store_true",
        help="compile to IR but don't save to BudeBWF file"
    )
    compile_parser.set_defaults(handler=compile_handler)
    args = arg_parser.parse_args()
    args.handler(args)


if __name__ == "__main__":
    main()
