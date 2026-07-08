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
    OPTION_SET_NAME = sys.argv[5]
    assert OPTION_SET_NAME in (
        "std-io",
        "zero-copy",
        "async",
        "async-zero-copy",
    ), "Expected OPTION_SET_NAME to be 'std-io', 'zero-copy', 'async', or 'async-zero-copy'"
    # async-zero-copy exercises the async (non-direct) Cow path: the reader owns
    # the bytes so decode reads into Cow::Owned; decode_direct (true zero-copy)
    # stays sync and is covered by the plain zero-copy set.
    is_async = OPTION_SET_NAME in ("async", "async-zero-copy")
    await_kw = ".await" if is_async else ""
    main_attr = '#[tokio::main(flavor = "current_thread")]\n' if is_async else ""
    main_kw = "async fn main" if is_async else "fn main"

    print(f"Testing {TEST_TARGET_FILE} with {INPUT_FILE} and {OUTPUT_FILE} (option_set={OPTION_SET_NAME})")

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
        if is_async:
            # Minimal features only: current-thread runtime + macros + async io
            # traits. Avoids tokio's networking stack (mio/WinSock) which is heavy
            # to compile and unnecessary for byte-buffer round-trip tests.
            f.write('tokio = { version = "1", default-features = false, features = ["rt", "macros", "io-util"] }\n')

    # Copy generated rust code as a library
    with open(TEST_TARGET_FILE, "r") as f_src:
        with open(src_dir / "lib.rs", "w") as f_dst:
            f_dst.write(f_src.read())

    # Create main.rs to run the test
    # For zero-copy option-set: use decode_direct(&data, &mut offset) against the raw slice.
    # For std-io option-set: use conventional Cursor+decode.
    if OPTION_SET_NAME == "zero-copy":
        decode_block = f"""    let mut target = test_runner::{TEST_TARGET_FORMAT}::default();
    let mut __offset: usize = 0;
    if let Err(e) = target.decode_direct(&input_data, &mut __offset) {{
        eprintln!("Decode error: {{:?}}", e);
        std::process::exit(10);
    }}"""
    else:
        decode_block = f"""    let mut reader = Cursor::new(&input_data);
    let mut target = test_runner::{TEST_TARGET_FORMAT}::default();
    if let Err(e) = target.decode(&mut reader){await_kw} {{
        eprintln!("Decode error: {{:?}}", e);
        std::process::exit(10);
    }}"""

    with open(src_dir / "main.rs", "w") as f:
        f.write(
            f"""
use std::fs;
use std::io::Cursor;
use std::env;
use test_runner;

{main_attr}{main_kw}() {{
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

{decode_block}

    let mut output_buf = Vec::new();
    if let Err(e) = target.encode(&mut Cursor::new(&mut output_buf)){await_kw} {{
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
