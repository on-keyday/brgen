#!/usr/bin/env python3
# Test logic for ebm2rust
import sys
import os
import subprocess
import pathlib
import json


def main():
    TEST_TARGET_FILE = sys.argv[1]
    INPUT_FILE = sys.argv[2]
    OUTPUT_FILE = sys.argv[3]
    TEST_TARGET_FORMAT = sys.argv[4]

    print(f"Testing {TEST_TARGET_FILE} with {INPUT_FILE} and {OUTPUT_FILE}")

    proj_dir = pathlib.Path("cargo_proj")
    src_dir = proj_dir / "src"
    os.makedirs(src_dir, exist_ok=True)

    # Create Cargo.toml
    with open(proj_dir / "Cargo.toml", "w") as f:
        f.write("[package]\n")
        f.write('name = "test_runner"\n')
        f.write('version = "0.1.0"\n')
        f.write('edition = "2021"\n')
        f.write("\n")
        f.write("[dependencies]\n")
        f.write('anyhow = "1.0"\n')

    # Copy generated rust code as a library
    with open(TEST_TARGET_FILE, "r") as f_src:
        with open(src_dir / "lib.rs", "w") as f_dst:
            f_dst.write(f_src.read())

    # Create main.rs to run the test
    with open(src_dir / "main.rs", "w") as f:
        f.write(
            f"""
use std::fs;
use std::io::Cursor;
use std::env;
use test_runner;

fn main() {{
    let args: Vec<String> = env::args().collect();
    if args.len() < 3 {{
        eprintln!("Usage: {{}} <input_file> <output_file>", args[0]);
        std::process::exit(1);
    }}

    let input_path = &args[1];
    let output_path = &args[2];

    let input_data = match fs::read(input_path) {{
        Ok(data) => data,
        Err(e) => {{
            eprintln!("Failed to read input file '{{}}': {{}}", input_path, e);
            std::process::exit(1);
        }}
    }};
    let mut reader = Cursor::new(&input_data);


    let mut target = test_runner::{TEST_TARGET_FORMAT}::default();
    if let Err(e) = target.decode(&mut reader) {{
        eprintln!("Decode error: {{:?}}", e);
        std::process::exit(10);
    }}

    let mut output_buf = Vec::new();
    if let Err(e) = target.encode(&mut Cursor::new(&mut output_buf)) {{
        eprintln!("Encode error: {{:?}}", e);
        std::process::exit(20);
    }}
    
    if let Err(e) = fs::write(output_path, &output_buf) {{
        eprintln!("Failed to write output file '{{}}': {{}}", output_path, e);
        std::process::exit(1);
    }}

    // success
    std::process::exit(0);
}}
"""
        )

    # Compile the rust code
    print("\nCompiling generated rust code...")
    result = subprocess.run(
        ["cargo", "build", "--color=never"],
        cwd=proj_dir,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print("Rust compilation failed!")
        print(result.stdout)
        print(result.stderr)
        sys.exit(1)

    print("Compilation successful.")
    # Run the compiled test executable
    executable_path = proj_dir / "target" / "debug" / "test_runner"
    if os.name == "nt":
        executable_path = executable_path.with_suffix(".exe")
    print("for VSCode debugging")
    print(
        json.dumps(
            {
                "type": "cppvsdbg" if os.name == "nt" else "cppdbg",
                "request": "launch",
                "cwd": os.getcwd(),
                "name": f"Debug ebm2rust unictest ({TEST_TARGET_FORMAT})",
                "program": os.path.abspath(executable_path),
                "args": [
                    INPUT_FILE,
                    OUTPUT_FILE,
                ],
            },
            indent=4,
        )
    )

    print(f"\nRunning compiled test: {executable_path}")
    proc = subprocess.run(
        [
            str(executable_path),
            INPUT_FILE,
            OUTPUT_FILE,
            TEST_TARGET_FORMAT,
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
        print(f"Test executable failed with exit code {{proc.returncode}}")

    sys.exit(proc.returncode)


if __name__ == "__main__":
    main()
