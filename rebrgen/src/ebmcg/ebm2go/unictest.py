#!/usr/bin/env python3
# Test logic for ebm2go
import sys
import os
import subprocess
import pathlib
import json

import unictest_report


def main():
    TEST_TARGET_FILE = sys.argv[1]  # The generated .go file
    INPUT_FILE = sys.argv[2]
    OUTPUT_FILE = sys.argv[3]
    TEST_TARGET_FORMAT = sys.argv[4]  # The struct name (CamelCase)
    OPTION_SET_NAME = sys.argv[5]
    EXTRA_FLAGS = sys.argv[6:]
    assert OPTION_SET_NAME in (
        "std-io",
        "slice-io",
        "append-io",
        "bit-stream",
    ), "Expected OPTION_SET_NAME to be 'std-io', 'slice-io', 'append-io' or 'bit-stream'"
    # very_slow_bit_ops: fallback terminals present -> only the offset-0
    # differential is valid; shifted round-trips are skipped
    vslow_offset0_only = "--vslow-offset0-only" in EXTRA_FLAGS

    print(f"Testing {TEST_TARGET_FILE} with {INPUT_FILE} and {OUTPUT_FILE}")

    proj_dir = pathlib.Path("go_proj")
    os.makedirs(proj_dir, exist_ok=True)

    # Copy generated Go code into the project directory
    with open(TEST_TARGET_FILE, "r") as f_src:
        with open(proj_dir / "generated.go", "w") as f_dst:
            f_dst.write(f_src.read())

    # Create go.mod
    with open(proj_dir / "go.mod", "w") as f:
        f.write("module test_runner\n\n")
        f.write("go 1.21\n")

    # Create main.go test harness
    #
    # The generated Go code uses io.Reader/io.Writer for decoder and encoder IO:
    #   func (s *Struct) Decode(r io.Reader) error
    #   func (s *Struct) Encode(w io.Writer) error
    #
    # Decode: wraps input data in bytes.NewReader.
    # Encode: writes to bytes.Buffer, then extracts bytes.
    io_pattern = {
        "std-io": {
            "decode": "reader := bytes.NewReader(inputData)\nerr = target.Read(reader)",
            "encode": "var buf bytes.Buffer\nerr = target.Write(&buf)\noutputData := buf.Bytes()",
        },
        "slice-io": {
            "decode": "_, err = target.Decode(inputData)",
            "encode": "outputData,err := target.Encode(make([]byte, len(inputData)))",
        },
        "append-io": {
            "decode": "_, err = target.Decode(inputData)",
            "encode": "outputData, err := target.Append(make([]byte, 0, len(inputData)))",
        },
    }

    if OPTION_SET_NAME == "bit-stream":
        write_bit_stream_harness(
            proj_dir, TEST_TARGET_FORMAT, vslow_offset0_only
        )
    else:
        write_default_harness(proj_dir, TEST_TARGET_FORMAT, OPTION_SET_NAME, io_pattern)

    build_and_run(proj_dir, TEST_TARGET_FORMAT, INPUT_FILE, OUTPUT_FILE)


VSLOW_SHIFTED_SECTION = """	// shifted round-trip: decode/encode each python-shifted input at bit offset k
	for k := uint8(1); k < 8; k++ {
		p := filepath.Join(filepath.Dir(inputPath), fmt.Sprintf("shifted_input_%d.bin", k))
		shifted, err := os.ReadFile(p)
		if err != nil {
			fmt.Fprintf(os.Stderr, "vslow: read %s: %v\\n", p, err)
			os.Exit(30)
		}
		var st __FORMAT__
		dOff := 1
		drs := RuntimeState{CurrentBit: k, CurrentByte: shifted[0]}
		if err := st.DecodeBitStream(shifted, &dOff, &drs); err != nil {
			fmt.Fprintf(os.Stderr, "vslow decode(k=%d) error: %v\\n", k, err)
			os.Exit(30)
		}
		if !reflect.DeepEqual(target, st) {
			fmt.Fprintf(os.Stderr, "vslow decode(k=%d) struct mismatch\\n byte-IO: %+v\\n bit    : %+v\\n", k, target, st)
			os.Exit(30)
		}
		sbuf := make([]byte, len(shifted)+16)
		sOff := 0
		srs := RuntimeState{CurrentBit: k}
		if err := st.EncodeBitStream(sbuf, &sOff, &srs); err != nil {
			fmt.Fprintf(os.Stderr, "vslow encode(k=%d) error: %v\\n", k, err)
			os.Exit(30)
		}
		if srs.CurrentBit != 0 {
			sbuf[sOff] = srs.CurrentByte
			sOff++
		}
		op := filepath.Join(filepath.Dir(outputPath), fmt.Sprintf("output_shifted_%d.bin", k))
		if err := os.WriteFile(op, sbuf[:sOff], 0644); err != nil {
			fmt.Fprintf(os.Stderr, "vslow: write %s: %v\\n", op, err)
			os.Exit(1)
		}
	}
"""

VSLOW_HARNESS_TEMPLATE = """package main

import (
	"bytes"
	"fmt"
	"os"
	"path/filepath"
	"reflect"
)

var _ = filepath.Join // keep import used even without the shifted section

func main() {
	if len(os.Args) < 3 {
		fmt.Fprintf(os.Stderr, "Usage: %s <input_file> <output_file>\\n", os.Args[0])
		os.Exit(1)
	}
	inputPath := os.Args[1]
	outputPath := os.Args[2]

	inputData, err := os.ReadFile(inputPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to read input file '%s': %v\\n", inputPath, err)
		os.Exit(1)
	}

	// byte-IO decode (reference)
	var target __FORMAT__
	inOff := 0
	err = target.DecodeSlice(inputData, &inOff)
	if err != nil {
		// byte-IO rejected the input; the bit-stream variant must reject it too
		var bt __FORMAT__
		bOff := 0
		brs := RuntimeState{}
		if berr := bt.DecodeBitStream(inputData, &bOff, &brs); berr == nil {
			fmt.Fprintf(os.Stderr, "vslow divergence: byte-IO decode failed (%v) but bit-stream decode succeeded\\n", err)
			os.Exit(30)
		}
		fmt.Fprintf(os.Stderr, "Decode error: %v\\n", err)
		os.Exit(10)
	}

	// byte-IO encode (reference bytes; also the framework round-trip output)
	outBuf := make([]byte, len(inputData)+16)
	outOff := 0
	err = target.EncodeSlice(outBuf, &outOff)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Encode error: %v\\n", err)
		os.Exit(20)
	}
	outputData := outBuf[:outOff]

	// offset-0 differential: bit-stream decode/encode must match byte-IO
	{
		var bitTarget __FORMAT__
		bOff := 0
		rs := RuntimeState{}
		if err := bitTarget.DecodeBitStream(inputData, &bOff, &rs); err != nil {
			fmt.Fprintf(os.Stderr, "vslow decode(k=0) error: %v\\n", err)
			os.Exit(30)
		}
		if !reflect.DeepEqual(target, bitTarget) {
			fmt.Fprintf(os.Stderr, "vslow decode(k=0) struct mismatch\\n byte-IO: %+v\\n bit    : %+v\\n", target, bitTarget)
			os.Exit(30)
		}
		if bOff != inOff || rs.CurrentBit != 0 {
			fmt.Fprintf(os.Stderr, "vslow decode(k=0) cursor mismatch: byteOff=%d bitOff=%d currentBit=%d\\n", inOff, bOff, rs.CurrentBit)
			os.Exit(30)
		}
		ebuf := make([]byte, len(inputData)+16)
		eOff := 0
		ers := RuntimeState{}
		if err := target.EncodeBitStream(ebuf, &eOff, &ers); err != nil {
			fmt.Fprintf(os.Stderr, "vslow encode(k=0) error: %v\\n", err)
			os.Exit(30)
		}
		if ers.CurrentBit != 0 {
			ebuf[eOff] = ers.CurrentByte
			eOff++
		}
		if !bytes.Equal(ebuf[:eOff], outputData) {
			fmt.Fprintf(os.Stderr, "vslow encode(k=0) bytes mismatch\\n byte-IO: %x\\n bit    : %x\\n", outputData, ebuf[:eOff])
			os.Exit(30)
		}
	}

__SHIFTED__
	err = os.WriteFile(outputPath, outputData, 0644)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to write output file '%s': %v\\n", outputPath, err)
		os.Exit(1)
	}

	os.Exit(0)
}
"""


def write_bit_stream_harness(proj_dir, format_name, offset0_only):
    shifted = "" if offset0_only else VSLOW_SHIFTED_SECTION
    code = VSLOW_HARNESS_TEMPLATE.replace("__FORMAT__", format_name).replace(
        "__SHIFTED__", shifted.replace("__FORMAT__", format_name)
    )
    with open(proj_dir / "main.go", "w") as f:
        f.write(code)


def write_default_harness(proj_dir, TEST_TARGET_FORMAT, OPTION_SET_NAME, io_pattern):
    with open(proj_dir / "main.go", "w") as f:
        f.write(
            f"""package main

import (
	{"\"bytes\"" if OPTION_SET_NAME == "std-io" else ""}
	"fmt"
	"os"
)

func main() {{
	if len(os.Args) < 3 {{
		fmt.Fprintf(os.Stderr, "Usage: %s <input_file> <output_file>\\n", os.Args[0])
		os.Exit(1)
	}}

	inputPath := os.Args[1]
	outputPath := os.Args[2]

	// Read input file
	inputData, err := os.ReadFile(inputPath)
	if err != nil {{
		fmt.Fprintf(os.Stderr, "Failed to read input file '%s': %v\\n", inputPath, err)
		os.Exit(1)
	}}

	// Decode
	var target {TEST_TARGET_FORMAT}
	{io_pattern[OPTION_SET_NAME]["decode"]}
	if err != nil {{
		fmt.Fprintf(os.Stderr, "Decode error: %v\\n", err)
		os.Exit(10)
	}}

	// Encode
	{io_pattern[OPTION_SET_NAME]["encode"]}
	if err != nil {{
		fmt.Fprintf(os.Stderr, "Encode error: %v\\n", err)
		os.Exit(20)
	}}

	// Write output file
	err = os.WriteFile(outputPath, outputData, 0644)
	if err != nil {{
		fmt.Fprintf(os.Stderr, "Failed to write output file '%s': %v\\n", outputPath, err)
		os.Exit(1)
	}}

	os.Exit(0)
}}
"""
        )


def build_and_run(proj_dir, TEST_TARGET_FORMAT, INPUT_FILE, OUTPUT_FILE):
    # Build the Go project
    print("\nBuilding Go project...")
    result = subprocess.run(
        [
            "go",
            "build",
            "-o",
            "test_runner.exe" if os.name == "nt" else "test_runner",
            ".",
        ],
        cwd=proj_dir,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print("Go compilation failed!")
        print(result.stdout)
        print(result.stderr)
        unictest_report.fail("compile", (result.stdout or "") + (result.stderr or ""))

    print("Compilation successful.")

    # Run the compiled test executable
    executable_name = "test_runner.exe" if os.name == "nt" else "test_runner"
    executable_path = (proj_dir / executable_name).resolve()

    print("for VSCode debugging")
    print(
        json.dumps(
            {
                "type": "go",
                "request": "launch",
                "cwd": os.getcwd(),
                "name": f"Debug ebm2go unictest ({TEST_TARGET_FORMAT})",
                "program": proj_dir.absolute().as_posix(),
                "args": [
                    INPUT_FILE,
                    OUTPUT_FILE,
                ],
                "stopOnEntry": True,
            },
            indent=4,
        )
    )

    print(f"\nRunning compiled test: {executable_path.as_posix()}")
    proc = subprocess.run(
        [
            executable_path.as_posix(),
            INPUT_FILE,
            OUTPUT_FILE,
        ],
        capture_output=True,
        text=True,
    )

    if proc.stdout:
        print("--- stdout ---")
        print(proc.stdout)
    if proc.stderr:
        print("--- stderr ---")
        print(proc.stderr)

    if proc.returncode != 0:
        print(f"Test executable failed with exit code {proc.returncode}")
        # The harness (main.go) prints "Decode error: X" / "Encode error: X"
        # and exits 10 / 20; 30 is a very_slow_bit_ops differential divergence.
        phase = {10: "decode", 20: "encode", 30: "vslow-diff"}.get(proc.returncode, "run")
        unictest_report.fail(phase, proc.stderr, code=proc.returncode)

    sys.exit(proc.returncode)


if __name__ == "__main__":
    main()
