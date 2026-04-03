#!/usr/bin/env python3
# Test logic for ebm2rmw
import sys
import os
import subprocess
import pathlib as pl
import json


def main():
    TEST_TARGET_FILE = sys.argv[1]  # text file containing the path to runner_input.ebm
    INPUT_FILE = sys.argv[2]
    OUTPUT_FILE = sys.argv[3]
    TEST_TARGET_FORMAT = sys.argv[4]
    # sys.argv[5] is optionset_name (unused here)
    # sys.argv[6:] are additional run options forwarded from UNICTEST_OPTION_SET_RUN_OPTIONS
    additional_args = [a for a in sys.argv[6:] if a]

    # Setup wrote the EBM file path as plain text into TEST_TARGET_FILE
    with open(TEST_TARGET_FILE, "r") as f:
        ebm_file = f.read().strip()

    original_workdir = pl.Path(os.environ["UNICTEST_ORIGINAL_WORK_DIR"])
    ebm2rmw = original_workdir / "tool" / "ebm2rmw"
    if os.name == "nt":
        ebm2rmw = ebm2rmw.with_suffix(".exe")

    cmd = [
        ebm2rmw.as_posix(),
        "-i", ebm_file,
        "--entry-point", TEST_TARGET_FORMAT,
        "--binary-file", INPUT_FILE,
        "--output-file", OUTPUT_FILE,
    ]
    cmd.extend(additional_args)

    print(f"Running: {' '.join(cmd)}", flush=True)

    print("for VSCode debugging")
    print(json.dumps({
        "type": "cppvsdbg" if os.name == "nt" else "cppdbg",
        "request": "launch",
        "cwd": os.getcwd(),
        "name": f"Debug ebm2rmw unictest ({TEST_TARGET_FORMAT})",
        "program": ebm2rmw.as_posix(),
        "args": [
            "-i", ebm_file,
            "--entry-point", TEST_TARGET_FORMAT,
            "--binary-file", INPUT_FILE,
            "--output-file", OUTPUT_FILE,
        ] + additional_args,
        "stopAtEntry": True,
    }, indent=4))

    result = subprocess.run(cmd, stdout=sys.stdout, stderr=sys.stderr)
    sys.exit(result.returncode)


if __name__ == "__main__":
    main()
