#!/usr/bin/env python3
"""Example manager -- build, run, etc., all the Bude examples in the 'examples' directory."""

import argparse
import os
import pathlib
import subprocess
import tempfile


EXAMPLE_DIR = pathlib.Path("../examples/").resolve()
OUTPUT_DIR = pathlib.Path("./output").resolve()
BUDE_SOURCES = EXAMPLE_DIR.glob("*.bude")
BUDE_EXE = pathlib.Path("../bin/bude.exe").resolve()

log_level = 0


def log_with_level(target_level, *print_args, **print_kwargs) -> None:
    if log_level >= target_level:
        print(*print_args, **print_kwargs)


def compile_handler(args: argparse.Namespace) -> None:
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


def asm_handler(args: argparse.Namespace) -> None:
    error_level = 0
    for filename in BUDE_SOURCES:
        bude_args = ["-a"]
        name, _, ext = filename.name.rpartition(".")
        if name and ext != "bude":
            continue  # Skip non-Bude files.
        if not args.no_write:
            new_name = ".".join((name, "asm"))
            output_path = OUTPUT_DIR / new_name
            bude_args.extend(["-o", str(output_path)])
        proc = subprocess.run([str(BUDE_EXE), str(filename), *bude_args],
                              capture_output=True)
        log_with_level(2, *proc.args)
        exit_status = proc.returncode
        error_level += abs(exit_status)
        if exit_status == 0:
            log_with_level(-1, f"File {filename.name!r}" \
                           f" produced assembly successfully ({exit_status}):")
            log_with_level(1, proc.stdout.decode())
        else:
            log_with_level(-1, f"File {filename.name!r}" \
                           f" failed to produce assembly ({exit_status}):")
            log_with_level(0, proc.stderr.decode())
    if error_level == 0:
        log_with_level(-2, "Whole suite successfully produced assembly!")
    else:
        log_with_level(-2, "Suite produced assembly with errors.")
        exit(1)


def build_handler(args: argparse.Namespace) -> None:
    error_level = 0
    with tempfile.TemporaryDirectory() as tdir:
        for filename in BUDE_SOURCES:
            name, _, ext = filename.name.rpartition(".")
            if name and ext != "bude":
                continue  # Skip non-Bude files.
            asm_name = ".".join((name, "asm"))
            asm_path = pathlib.Path(tdir) / asm_name
            exe_name = ".".join((name, "exe"))
            output_path = OUTPUT_DIR / exe_name
            bude_args = ["-a", "-o", asm_path]
            bude_proc = subprocess.run([str(BUDE_EXE), str(filename), *bude_args],
                                       capture_output=True)
            log_with_level(2, *bude_proc.args)
            bude_exit_status = bude_proc.returncode
            error_level += abs(bude_exit_status)
            if bude_exit_status == 0:
                fasm_proc = subprocess.run(["fasm", asm_path, output_path],
                                           capture_output=True)
                log_with_level(2, *fasm_proc.args)
                fasm_exit_status = fasm_proc.returncode
                error_level += abs(fasm_exit_status)
                if fasm_exit_status == 0:
                    log_with_level(-1, f"File {filename.name!r} built successfully:")
                    log_with_level(1, fasm_proc.stdout.decode())
                else:
                    log_with_level(-1, f"File {filename.name!r} failed to assemble " \
                                   f"({fasm_exit_status}):")
                    log_with_level(0, fasm_proc.stderr.decode())
            else:
                log_with_level(-1, "File {filename!r} failed to produce assembly" \
                               f" ({bude_exit_status}):")
                log_with_level(0, bude_proc.stderr.decode())
    if error_level == 0:
        log_with_level(-2, "Whole suite built successfully!")
    else:
        log_with_level(-2, "Suite built with errors.")
        exit(1)


def run_handler(args: argparse.Namespace) -> None:
    pass


def main() -> None:
    arg_parser = argparse.ArgumentParser(description=__doc__)
    arg_parser.add_argument("--message-level", action="store",
                            type=int, choices=range(-2, 3), default=0,
                            help="level of diagnostic messages to emit (lower -> fewer messages)")
    subparsers = arg_parser.add_subparsers(title="subcommands", required=True)
    compile_help = "compile files to word-oriented IR code"
    compile_parser = subparsers.add_parser(
        "compile",
        description=f"Compile subcommand -- {compile_help}.",
        help=compile_help
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
    asm_help = "produce assembly code"
    asm_parser = subparsers.add_parser(
        "asm",
        description=f"Asm subcommand -- {asm_help}.",
        help=asm_help
    )
    asm_parser.add_argument(
        "-n", "--no-write", action="store_true",
        help="produce assembly code but don't save to BudeBWF file"
    )
    asm_parser.set_defaults(handler=asm_handler)
    build_help = "build executable by assembling asm ouput with FASM"
    build_parser = subparsers.add_parser(
        "build",
        description=f"Build subcommand -- {build_help}.",
        help=build_help
    )
    build_parser.set_defaults(handler=build_handler)
    run_help = "run files in Bude interpreter"
    run_parser = subparsers.add_parser(
        "run",
        description=f"Run subcommand -- {run_help}.",
        help=run_help
    )
    run_parser.set_defaults(handler=run_handler)
    args = arg_parser.parse_args()
    global log_level
    log_level = args.message_level
    args.handler(args)


if __name__ == "__main__":
    main()
