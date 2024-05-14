#!/usr/bin/env python3
"""Example manager -- build, run, etc., all the Bude examples in the 'examples' directory."""

import argparse
import pathlib
import subprocess


EXAMPLE_DIR = pathlib.Path("../examples/").resolve()
OUTPUT_DIR = pathlib.Path("./output").resolve()
BUDE_SOURCES = EXAMPLE_DIR.glob("*.bude")
BUDE_EXE = pathlib.Path("../bin/bude.exe").resolve()

log_level = 0


def log_with_level(target_level, *print_args, **print_kwargs):
    if log_level >= target_level:
        print(*print_args, **print_kwargs)


def compile_handler(args: argparse.Namespace):
    error_level = 0
    for filename in BUDE_SOURCES:
        bude_args = ["-b"]
        name, _, ext = filename.name.rpartition(".")
        if name and ext != "bude":
            continue  # Skip non-Bude files. Filenames '[.]bude' are also skipped.
        if args.dump:
            bude_args.append("-d")
        if not args.no_write:
            new_name = ".".join((name, "bbwf"))
            output_path = OUTPUT_DIR / new_name
            bude_args.extend(["-o", str(output_path)])
        proc = subprocess.run([str(BUDE_EXE), str(filename), *bude_args],
                              capture_output=True)
        log_with_level(2, *proc.args)
        exit_status = proc.returncode
        error_level += abs(exit_status)
        if exit_status == 0:
            log_with_level(-1, f"File {filename.name!r} compiled successfully ({exit_status}):")
            log_with_level(1, proc.stdout.decode())
        else:
            log_with_level(-1, f"File {filename.name!r} failed to compile ({exit_status}):")
            log_with_level(0, proc.stderr.decode())

    if error_level == 0:
        log_with_level(-2, "Whole suite compiled successfully!")
    else:
        log_with_level(-2, "Suite compiled with errors.")
        exit(1)


def main():
    arg_parser = argparse.ArgumentParser(description=__doc__)
    arg_parser.add_argument("--message-level", action="store",
                            type=int, choices=range(-2, 3), default=0,
                            help="level of diagnostic messages to emit (lower -> fewer messages)")
    subparsers = arg_parser.add_subparsers(title="subcommands", required=True)
    compile_parser = subparsers.add_parser(
        "compile", help="compile files to word-oriented IR code"
    )
    compile_parser.add_argument(
        "-d", "--dump", action="store_true",
        help="dump IR code to stdout"
    )
    compile_parser.add_argument(
        "-n", "--no-write", action="store_true",
        help="compile to IR but don't save to BudeBWF file"
    )
    compile_parser.set_defaults(handler=compile_handler)
    args = arg_parser.parse_args()
    global log_level
    log_level = args.message_level
    args.handler(args)


if __name__ == "__main__":
    main()
