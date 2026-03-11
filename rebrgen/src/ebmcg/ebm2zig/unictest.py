#!/usr/bin/env python3
# Test logic for ebm2zig
import sys
import os
import subprocess
import pathlib as pl
import json
import shutil


def main():
    TEST_TARGET_FILE = sys.argv[1]  # The generated .zig file
    INPUT_FILE = sys.argv[2]
    OUTPUT_FILE = sys.argv[3]
    TEST_TARGET_FORMAT = sys.argv[4]  # The struct name (PascalCase)

    print(f"Testing {TEST_TARGET_FILE} with {INPUT_FILE} and {OUTPUT_FILE}")

    proj_dir = pl.Path("zig_proj")
    os.makedirs(proj_dir, exist_ok=True)

    # Copy generated Zig code into the project directory
    shutil.copy2(TEST_TARGET_FILE, proj_dir / "generated.zig")

    # Create main.zig test harness
    #
    # The generated Zig code uses a reader/writer pattern for decoder and encoder IO:
    #   pub fn decode(self: *Struct, reader: anytype) !void
    #   pub fn encode(self: *const Struct, writer: anytype) !void
    #
    # Decode: wraps input data in a fixedBufferStream reader.
    # Encode: writes to an ArrayList writer, then extracts bytes.
    with open(proj_dir / "main.zig", "w") as f:
        f.write(
            f"""const std = @import("std");
const generated = @import("generated.zig");

pub fn main() !void {{
    var gpa = std.heap.GeneralPurposeAllocator(.{{}}){{}}; 
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const args = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args);

    if (args.len < 3) {{
        std.debug.print("Usage: {{s}} <input_file> <output_file>\\n", .{{args[0]}});
        std.process.exit(1);
    }}

    const input_path = args[1];
    const output_path = args[2];

    // Read input file
    const input_data = std.fs.cwd().readFileAlloc(allocator, input_path, std.math.maxInt(usize)) catch |err| {{
        std.debug.print("Failed to read input file '{{s}}': {{}}\\n", .{{ input_path, err }});
        std.process.exit(1);
    }};
    defer allocator.free(input_data);

    // Decode
    var target: generated.{TEST_TARGET_FORMAT} = .{{}};
    var read_stream = std.io.fixedBufferStream(input_data);
    target.decode(allocator, read_stream.reader()) catch |err| {{
        std.debug.print("Decode error: {{}}\\n", .{{err}});
        std.process.exit(10);
    }};
    defer target.deinit(allocator);

    // Encode
    var output_buf = std.ArrayList(u8).init(allocator);
    defer output_buf.deinit();
    target.encode(output_buf.writer()) catch |err| {{
        std.debug.print("Encode error: {{}}\\n", .{{err}});
        std.process.exit(20);
    }};

    // Write output file
    const out_file = std.fs.cwd().createFile(output_path, .{{}}) catch |err| {{
        std.debug.print("Failed to create output file '{{s}}': {{}}\\n", .{{ output_path, err }});
        std.process.exit(1);
    }};
    defer out_file.close();
    out_file.writeAll(output_buf.items) catch |err| {{
        std.debug.print("Failed to write output file '{{s}}': {{}}\\n", .{{ output_path, err }});
        std.process.exit(1);
    }};

    std.process.exit(0);
}}
"""
        )

    # Build the Zig project
    print("\nBuilding Zig project...")
    executable_name = "test_runner.exe" if os.name == "nt" else "test_runner"
    result = subprocess.run(
        [
            "zig",
            "build-exe",
            "main.zig",
            f"-femit-bin={executable_name}",
        ],
        cwd=proj_dir,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print("Zig compilation failed!")
        if result.stdout:
            print(result.stdout)
        if result.stderr:
            print(result.stderr)
        sys.exit(1)

    print("Compilation successful.")

    # Run the compiled test executable
    executable_path = (proj_dir / executable_name).resolve()

    print("for VSCode debugging")
    print(
        json.dumps(
            {
                "type": "cppvsdbg" if os.name == "nt" else "cppdbg",
                "request": "launch",
                "cwd": os.getcwd(),
                "name": f"Debug ebm2zig unictest ({TEST_TARGET_FORMAT})",
                "program": executable_path.as_posix(),
                "args": [
                    INPUT_FILE,
                    OUTPUT_FILE,
                ],
                "stopAtEntry": True,
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

    sys.exit(proc.returncode)


if __name__ == "__main__":
    main()
