#!/usr/bin/env python3
# Test logic for ebm2ruby
import sys
import os
import subprocess
import json
import shutil


def main():
    TEST_TARGET_FILE = sys.argv[1]
    INPUT_FILE = sys.argv[2]
    OUTPUT_FILE = sys.argv[3]
    TEST_TARGET_FORMAT = sys.argv[4]

    print(f"Testing {TEST_TARGET_FILE} with {INPUT_FILE} and {OUTPUT_FILE}")

    ruby_bin = shutil.which("ruby") or "ruby"

    # Use forward slashes for Ruby path compatibility on Windows
    target_path = os.path.abspath(TEST_TARGET_FILE).replace("\\", "/")
    target_path_repr = repr(target_path)

    ruby_script = f"""require 'stringio'
load {target_path_repr}

input = StringIO.new(File.binread(ARGV[0]).b)
target = {TEST_TARGET_FORMAT}.new
begin
  target.decode(input)
rescue => e
  $stderr.puts "Decode error: #{{e.message}}"
  $stderr.puts e.backtrace.join("\\n")
  exit 10
end

output = StringIO.new
begin
  target.encode(output)
rescue => e
  $stderr.puts "Encode error: #{{e.message}}"
  $stderr.puts e.backtrace.join("\\n")
  exit 20
end

File.binwrite(ARGV[1], output.string)
"""

    test_script = "test_harness.rb"
    with open(test_script, "w", encoding="utf-8") as f:
        f.write(ruby_script)

    print("for VSCode debugging")
    print(
        json.dumps(
            {
                "type": "ruby_lsp",
                "request": "launch",
                "cwd": os.getcwd(),
                "name": f"Debug ebm2ruby unictest ({TEST_TARGET_FORMAT})",
                "program": os.path.abspath(test_script),
                "args": [INPUT_FILE, OUTPUT_FILE],
            },
            indent=4,
        )
    )

    print(f"\nRunning test harness: {test_script}", flush=True)
    proc = subprocess.run(
        [ruby_bin, test_script, INPUT_FILE, OUTPUT_FILE],
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
        print(f"Test script failed with exit code {proc.returncode}")

    sys.exit(proc.returncode)


if __name__ == "__main__":
    main()
