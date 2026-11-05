#!/usr/bin/env python3
# Test logic for ebm2hike
#
# The generated Hike code carries its own ebmDecoder/ebmEncoder prelude and
# imports nothing, so the harness below is a second file in the same package.
# Hike has no argv of its own yet, so the input and output paths are baked into
# the harness as string literals rather than read from the command line.
import json
import os
import pathlib as pl
import shutil
import subprocess
import sys

import unictest_report

HARNESS_TEMPLATE = """package main

func fopen(filename string, mode string) *byte
func fclose(stream *byte) int
func fread(ptr *byte, size int, nmemb int, stream *byte) int
func fwrite(ptr *byte, size int, nmemb int, stream *byte) int
func fseek(stream *byte, offset int, whence int) int
func ftell(stream *byte) int

func ebmTestReadAll(path string) ([]byte, int) {{
\tfp := fopen(path, "rb")
\tif fp == nil {{
\t\tvar empty []byte
\t\treturn empty, -1
\t}}
\tfseek(fp, 0, 2)
\tsize := ftell(fp)
\tfseek(fp, 0, 0)
\tbuf := make([]byte, size)
\tif size > 0 {{
\t\tfread(&buf[0], 1, size, fp)
\t}}
\tfclose(fp)
\treturn buf, 0
}}

func ebmTestWriteAll(path string, data []byte) int {{
\tfp := fopen(path, "wb")
\tif fp == nil {{
\t\treturn -1
\t}}
\tif len(data) > 0 {{
\t\tfwrite(&data[0], 1, len(data), fp)
\t}}
\tfclose(fp)
\treturn 0
}}

func main() int {{
\tinput, readErr := ebmTestReadAll("{input_path}")
\tif readErr != 0 {{
\t\treturn 1
\t}}
\tvar dec ebmDecoder = ebmNewDecoder(input)
\tvar target {format_name}
\tif target.Decode(&dec) != 0 {{
\t\treturn 10
\t}}
\tvar enc ebmEncoder = ebmNewEncoder()
\tif target.Encode(&enc) != 0 {{
\t\treturn 20
\t}}
\tif ebmTestWriteAll("{output_path}", ebmEncoderBytes(&enc)) != 0 {{
\t\treturn 1
\t}}
\treturn 0
}}
"""


HERE = pl.Path(__file__).parent
HIKEC_NAME = "hikec.exe" if os.name == "nt" else "hikec"


def find_hikec() -> str:
    # HIKEC points at the binary; HIKE_ROOT at a hike-lang checkout with one
    # built at its root (that is also where hikec resolves `std/` from).
    # .hike-lang is where dependency_setup.py puts it.
    explicit = os.environ.get("HIKEC")
    if explicit and pl.Path(explicit).exists():
        return explicit
    candidates = []
    root = os.environ.get("HIKE_ROOT")
    if root:
        candidates.append(pl.Path(root) / HIKEC_NAME)
    candidates.append(HERE / ".hike-lang" / HIKEC_NAME)
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve().as_posix()
    found = shutil.which("hikec")
    if found:
        return found
    unictest_report.fail(
        "setup",
        "hikec not found. Run src/ebmcg/ebm2hike/dependency_setup.py, or set "
        "HIKE_ROOT to a hike-lang checkout with hikec built at its root, or "
        "HIKEC to the compiler binary.",
    )


def main():
    TEST_TARGET_FILE = sys.argv[1]
    INPUT_FILE = sys.argv[2]
    OUTPUT_FILE = sys.argv[3]
    TEST_TARGET_FORMAT = sys.argv[4]

    print(f"Testing {TEST_TARGET_FILE} with {INPUT_FILE} and {OUTPUT_FILE}")

    hikec = find_hikec()

    # hikec compiles every .hike in the directory as one package, so the
    # project directory holds exactly the generated code plus the harness.
    proj_dir = pl.Path("hike_proj")
    if proj_dir.exists():
        shutil.rmtree(proj_dir)
    os.makedirs(proj_dir)
    shutil.copy2(TEST_TARGET_FILE, proj_dir / "generated.hike")

    harness = HARNESS_TEMPLATE.format(
        input_path=pl.Path(INPUT_FILE).resolve().as_posix(),
        output_path=pl.Path(OUTPUT_FILE).resolve().as_posix(),
        format_name=TEST_TARGET_FORMAT,
    )
    (proj_dir / "main.hike").write_text(harness, encoding="utf-8")

    executable_name = "test_runner.exe" if os.name == "nt" else "test_runner"
    print("\nBuilding Hike project...")
    result = subprocess.run(
        [hikec, "build", "-o", executable_name, "generated.hike", "main.hike"],
        cwd=proj_dir,
        capture_output=True,
        text=True,
    )
    # hikec panics rather than exiting non-zero on some semantic errors, so the
    # produced binary is what decides whether the build worked.
    executable_path = (proj_dir / executable_name).resolve()
    if result.returncode != 0 or not executable_path.exists():
        print("Hike compilation failed!")
        if result.stdout:
            print(result.stdout)
        if result.stderr:
            print(result.stderr)
        unictest_report.fail("compile", (result.stderr or "") + (result.stdout or ""))

    print("Compilation successful.")

    print("for VSCode debugging")
    print(
        json.dumps(
            {
                "type": "cppvsdbg" if os.name == "nt" else "cppdbg",
                "request": "launch",
                "cwd": os.getcwd(),
                "name": f"Debug ebm2hike unictest ({TEST_TARGET_FORMAT})",
                "program": executable_path.as_posix(),
                "args": [],
                "stopAtEntry": True,
            },
            indent=4,
        )
    )

    print(f"\nRunning compiled test: {executable_path.as_posix()}")
    proc = subprocess.run([executable_path.as_posix()], capture_output=True, text=True)

    if proc.stdout:
        print("--- stdout ---")
        print(proc.stdout)
    if proc.stderr:
        print("--- stderr ---")
        print(proc.stderr)

    if proc.returncode != 0:
        print(f"Test executable failed with exit code {proc.returncode}")
        # main() returns 10 on a decode error and 20 on an encode error.
        phase = {10: "decode", 20: "encode"}.get(proc.returncode, "run")
        unictest_report.fail(phase, proc.stderr, code=proc.returncode)

    sys.exit(proc.returncode)


if __name__ == "__main__":
    main()
