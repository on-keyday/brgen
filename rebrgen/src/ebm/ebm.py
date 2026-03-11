import subprocess
import os
import difflib

# This script is a Python port of ebm.ps1.
# It generates C++ header and source files for the Extended Binary Module (EBM)
# from a .bgn definition file.

# Absolute path to the brgen tool directory, as specified in the original script.
# This makes the script dependent on a specific directory structure.
try:
    with open("build_config.json") as fp:
        import json

        build_config = json.load(fp)
except FileNotFoundError:
    print("build_config.json not found, using default TOOL_PATH.")
    build_config = {}
import os

TOOL_PATH = os.path.join(build_config.get("BRGEN_DIR", "./brgen/"), "tool")

# Paths to the executables.
# We assume .exe extension for Windows, which is where the original .ps1 ran.
SRC2JSON_EXE = os.path.join(TOOL_PATH, "src2json")
JSON2CPP2_EXE = os.path.join(TOOL_PATH, "json2cpp2")
if os.name == "nt":
    SRC2JSON_EXE += ".exe"
    JSON2CPP2_EXE += ".exe"

# File paths (relative to the project root, where this script should be run from)
BGN_FILE = "src/ebm/extended_binary_module.bgn"
EBM_JSON_FILE = "save/ebm.json"
HPP_FILE = "src/ebm/extended_binary_module.hpp"
CPP_FILE = "src/ebm/extended_binary_module.cpp"
HPP_ZC_FILE = "src/ebm/extended_binary_module_zc.hpp"
CPP_ZC_FILE = "src/ebm/extended_binary_module_zc.cpp"


def run_command(command, output_file):
    """Runs a command and redirects its stdout to a file."""
    print(f"Running: {' '.join(command)} > {output_file}")
    # read output file if it exists
    if os.path.exists(output_file):
        with open(output_file, "r") as f:
            cached = f.read()
    else:
        cached = None
    try:
        result = subprocess.run(
            command, capture_output=True, check=True, text=True, encoding="utf-8"
        )
        if result.returncode != 0:
            print(f"Error running command. Return code: {result.returncode}")
            if result.stderr:
                print(f"Stderr: {result.stderr}")
            return False
        if result.stdout:
            new_content = result.stdout
            if new_content != cached:
                print(
                    f"Output changed: len(new_content) = {len(new_content)}, len(cached) = {len(cached) if cached else 'N/A'}"
                )
                difflib_context = difflib.unified_diff(
                    cached.splitlines(keepends=True) if cached else [],
                    new_content.splitlines(keepends=True),
                    fromfile="cached",
                    tofile="new",
                )
                print("".join(difflib_context))
                with open(output_file, "w") as f:
                    f.write(new_content)
                print(f"Wrote output to {output_file}")
            else:
                print(f"Output unchanged, not writing to {output_file}")
    except FileNotFoundError:
        print(f"Error: Command not found at {command[0]}")
        print("Please ensure the TOOL_PATH is correct and the brgen tools are built.")
        return False
    except subprocess.CalledProcessError as e:
        print(f"An error occurred while running {' '.join(command)}.")
        print(f"Return code: {e.returncode}")
        if e.stdout:
            print(f"Stdout: {e.stdout}")
        if e.stderr:
            print(f"Stderr: {e.stderr}")
        return False
    return True


zc_overrides = [
    "--bytes-override",
    "::futils::view::rvec",
    "--namespace-override",
    "ebm::zc",
]


def main():
    """Main function to execute the code generation steps."""
    print("Starting EBM C++ file generation...")

    # Step 1: Convert .bgn to brgen AST JSON
    # & $TOOL_PATH\src2json src/ebm/extended_binary_module.bgn | Out-File save/ebm.json -Encoding utf8
    cmd1 = [SRC2JSON_EXE, BGN_FILE]
    if not run_command(cmd1, EBM_JSON_FILE):
        return

    # Step 2: Generate C++ header file from JSON
    # & $TOOL_PATH\json2cpp2 -f save/ebm.json --mode header_file --add-visit --enum-stringer --use-error --dll-export | Out-File src/ebm/extended_binary_module.hpp -Encoding utf8
    cmd2 = [
        JSON2CPP2_EXE,
        "-f",
        EBM_JSON_FILE,
        "--mode",
        "header_file",
        "--add-visit",
        "--enum-stringer",
        "--use-error",
        "--dll-export",
    ]
    if not run_command(cmd2, HPP_FILE):
        return
    cmd2_zc = cmd2 + zc_overrides
    if not run_command(cmd2_zc, HPP_ZC_FILE):
        return

    # Step 3: Generate C++ source file from JSON
    # & $TOOL_PATH\json2cpp2 -f save/ebm.json --mode source_file --enum-stringer --use-error --dll-export | Out-File src/ebm/extended_binary_module.cpp -Encoding utf8
    cmd3 = [
        JSON2CPP2_EXE,
        "-f",
        EBM_JSON_FILE,
        "--mode",
        "source_file",
        "--enum-stringer",
        "--use-error",
        "--dll-export",
    ]
    if not run_command(cmd3, CPP_FILE):
        return
    cmd3_zc = cmd3 + zc_overrides
    if not run_command(cmd3_zc, CPP_ZC_FILE):
        return

    print("\nSuccessfully generated C++ files:")
    print(f"  - {HPP_FILE}")
    print(f"  - {CPP_FILE}")
    print(f"  - {HPP_ZC_FILE}")
    print(f"  - {CPP_ZC_FILE}")


if __name__ == "__main__":
    # Change directory to project root to handle relative paths correctly.
    # This script is in src/ebm, so the root is two levels up.
    project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    os.chdir(project_root)
    print(f"Working directory set to: {os.getcwd()}")
    main()
