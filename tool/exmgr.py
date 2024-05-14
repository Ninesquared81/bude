#!/usr/bin/env python3
"""Example manager -- build, run, etc. all the Bude examples in the example directory."""

import pathlib
import subprocess


def main():
    example_dir = pathlib.Path("../examples/").resolve()
    bude_sources = example_dir.glob("*.bude")
    bude_exe = pathlib.Path("../bin/bude.exe").resolve()

    error_level = 0
    for filename in bude_sources:
        proc = subprocess.run([str(bude_exe), str(filename), "-d"],
                              capture_output=True)
        exit_status = proc.returncode
        error_level += abs(exit_status)
        if exit_status == 0:
            print(f"File {filename.name!r} compiled successfully:")
            # print(proc.stdout.decode())
        else:
            print(f"File {filename.name!r} failed to compile:",
                  proc.stderr.decode(), sep="\n")

    if error_level == 0:
        print("Whole suite compiled successfully!")
    else:
        print("Suite compiled with errors.")
        exit(1)


if __name__ == "__main__":
    main()
