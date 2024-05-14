#!/usr/bin/env python3
"""Example manager -- build, run, etc., all the Bude examples in the 'examples' directory."""

import argparse
import pathlib
import subprocess


EXAMPLE_DIR = pathlib.Path("../examples/").resolve()
BUDE_SOURCES = EXAMPLE_DIR.glob("*.bude")
BUDE_EXE = pathlib.Path("../bin/bude.exe").resolve()


def compile_dump_ir(args: argparse.Namespace):
    error_level = 0
    for filename in BUDE_SOURCES:
        proc = subprocess.run([str(BUDE_EXE), str(filename), "-d"],
                              capture_output=True)
        exit_status = proc.returncode
        error_level += abs(exit_status)
        if exit_status == 0:
            print(f"File {filename.name!r} compiled successfully ({exit_status}):")
            if args.verbose:
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
    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument("-v", "--verbose", action="store_true", help="verbose mode")
    args = arg_parser.parse_args()
    compile_dump_ir(args)

if __name__ == "__main__":
    main()
