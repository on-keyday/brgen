import subprocess as sp
import sys
import os
import argparse
import json


sys.path.append(os.path.dirname(__file__))
from ebmtemplate import get_mode_dir
from util import execute


def do_default_dummy_header(lang_name: str, mode: str):
    TOOL_PATH = "tool/ebmcodegen"
    isInterpreter = mode == "interpret" or mode == "interpret-class"
    isClassBased = mode == "codegen-class" or mode == "interpret-class"

    DEFAULT_VISITOR_LOCATION = (
        f"src/ebmcodegen/default_{'interpret' if isInterpreter else 'codegen'}_visitor/"
    )
    VISITOR_LOCATION = os.path.join(DEFAULT_VISITOR_LOCATION, "visitor")
    os.makedirs(VISITOR_LOCATION, exist_ok=True)

    if isClassBased:
        CODE_GENERATOR_HEADER = execute(
            [
                TOOL_PATH,
                "--mode",
                mode + "-header",
                "--lang",
                lang_name,
                "--default-visitor-impl-dir",
                VISITOR_LOCATION,
            ],
            None,
        )
        TARGET_PATH = os.path.join(DEFAULT_VISITOR_LOCATION, "codegen.hpp")
        with open(TARGET_PATH, "rb") as f:
            existing_content = f.read() if os.path.exists(TARGET_PATH) else None
            if (
                existing_content is not None
                and existing_content == CODE_GENERATOR_HEADER
            ):
                print(
                    f"Default dummy header for {lang_name} in {TARGET_PATH} is already up to date."
                )
                return
        with open(TARGET_PATH, "wb") as f:
            f.write(CODE_GENERATOR_HEADER)
        print(
            f"Generated default dummy header for {lang_name} in {DEFAULT_VISITOR_LOCATION}/codegen.hpp"
        )
    else:
        print(f"No dummy header needed for non-class-based mode for {lang_name}")


TOOL_PATH = "tool/ebmcodegen"


def do_setup(lang_name: str, mode: str, file_extension: str):
    isInterpreter = mode == "interpret" or mode == "interpret-class"
    isClassBased = mode == "codegen-class" or mode == "interpret-class"
    if isInterpreter:
        PARENT_DIR_NAME = "ebmip"
    else:
        PARENT_DIR_NAME = "ebmcg"

    PROGRAM_NAME = "ebm2" + lang_name
    PARENT_CMAKE_PATH = f"src/{PARENT_DIR_NAME}/CMakeLists.txt"
    OUTPUT_DIR = f"src/{PARENT_DIR_NAME}/ebm2{lang_name}"
    TEST_CONFIG_PATH = "test/unictest.json"

    CMAKE = execute([TOOL_PATH, "--mode", "cmake", "--lang", lang_name], None)

    DEFAULT_VISITOR_LOCATION = f"ebmcodegen/default_{"interpret" if isInterpreter else "codegen"}_visitor/visitor/"

    if isClassBased:
        CODE_GENERATOR_HEADER = execute(
            [
                TOOL_PATH,
                "--mode",
                mode + "-header",
                "--lang",
                lang_name,
                "--default-visitor-impl-dir",
                DEFAULT_VISITOR_LOCATION,
            ],
            None,
        )
        CODE_GENERATOR = execute(
            [
                TOOL_PATH,
                "--mode",
                mode + "-source",
                "--lang",
                lang_name,
                "--default-visitor-impl-dir",
                DEFAULT_VISITOR_LOCATION,
            ],
            None,
        )
    else:
        CODE_GENERATOR_HEADER = None
        CODE_GENERATOR = execute([TOOL_PATH, "--mode", mode, "--lang", lang_name], None)

    os.makedirs(OUTPUT_DIR, exist_ok=True)
    VISITOR_DIR = os.path.join(OUTPUT_DIR, "visitor")
    os.makedirs(VISITOR_DIR, exist_ok=True)

    with open(os.path.join(OUTPUT_DIR, "CMakeLists.txt"), "wb") as f:
        f.write(CMAKE)
    with open(os.path.join(OUTPUT_DIR, "main.cpp"), "wb") as f:
        f.write(CODE_GENERATOR)
    if CODE_GENERATOR_HEADER is not None:
        with open(os.path.join(OUTPUT_DIR, "codegen.hpp"), "wb") as f:
            f.write(CODE_GENERATOR_HEADER)

    # add FILE_EXTENSIONS(ext) to Flags.hpp using script/ebmtemplate.py
    flags_path = os.path.join(VISITOR_DIR, "Flags.hpp")
    if not os.path.exists(flags_path):
        execute(
            [
                "python",
                (
                    "./script/ebmtemplate.py"
                    if not isInterpreter
                    else "./script/ebmtemplate_ip.py"
                ),
                "Flags",
                lang_name,
            ],
            None,
        )
        if not isInterpreter:
            with open(flags_path, "a") as f:
                f.write(f'\nFILE_EXTENSIONS("{file_extension}");\n')
            print(
                f"Added FILE_EXTENSIONS to {flags_path} with extension: {file_extension}"
            )
        else:
            print(f"Created {flags_path}")
    else:
        print(
            f"Flags.hpp already exists: {flags_path}, skipping FILE_EXTENSIONS addition."
        )

    # add test script file if not exists
    TEST_SCRIPT_PATH = os.path.join(OUTPUT_DIR, "unictest.py")
    if not os.path.exists(TEST_SCRIPT_PATH):
        with open(TEST_SCRIPT_PATH, "w") as f:
            f.write("#!/usr/bin/env python3\n")
            f.write("# Test logic for ebm2" + lang_name + "\n")
            f.write("import sys\n")
            f.write("import os\n")
            f.write("import json\n")
            f.write("import pathlib as pl\n\n")
            f.write("def main():\n")
            f.write("    TEST_TARGET_FILE = sys.argv[1]\n")
            f.write("    INPUT_FILE = sys.argv[2]\n")
            f.write("    OUTPUT_FILE = sys.argv[3]\n")
            f.write("    TEST_TARGET_FORMAT = sys.argv[4]\n")
            f.write("    # Test logic goes here\n")
            f.write(
                "    print(f'Testing {TEST_TARGET_FILE} with {INPUT_FILE} and {OUTPUT_FILE}')\n"
            )
            f.write("    # This is a placeholder for actual test implementation\n")
            f.write("    with open(INPUT_FILE, 'rb') as f:\n")
            f.write("        data = f.read()\n")
            f.write("    # Implement test logic based on TEST_TARGET_FORMAT\n")
            f.write(
                "    # Return 0 for success, 10 for decode error, 20 for encode error\n"
            )
            f.write("    # For demonstration, just write the same data to output\n")
            f.write("    with open(OUTPUT_FILE, 'wb') as f:\n")
            f.write("        f.write(data)\n")
            f.write("    print('Test logic is not implemented yet.')\n")
            f.write('    print("for VSCode debugging")\n')
            f.write('    executable_path = pl.Path("/path/to/executable")\n')
            f.write("    print(")
            f.write(
                f"""
                json.dumps(
                    {{
                        "type": "cppvsdbg" if os.name == "nt" else "cppdbg",
                        "request": "launch",
                        "cwd": os.getcwd(),
                        "name": f"Debug ebm2{lang_name} unictest ({{TEST_TARGET_FORMAT}})",
                        "program": executable_path.as_posix(),
                        "args": [
                            INPUT_FILE,
                            OUTPUT_FILE,
                        ],
                        "stopAtEntry": True,
                    }},
                    indent=4,
                )
"""
            )
            f.write("    )\n")
            f.write("    exit(1)\n")
            f.write("if __name__ == '__main__':\n")
            f.write("    main()\n")
        print(f"Created test script: {TEST_SCRIPT_PATH}")
    else:
        print(f"Test script already exists: {TEST_SCRIPT_PATH}")

    with open(TEST_CONFIG_PATH, "r") as f:
        test_config = json.load(f)

    runners = test_config.get("runners", [])

    new_runner_path = (
        "$WORK_DIR/src/"
        + PARENT_DIR_NAME
        + "/ebm2"
        + lang_name
        + "/unictest_runner.json"
    )

    actual_new_runner_path = os.path.join(
        "src", PARENT_DIR_NAME, "ebm2" + lang_name, "unictest_runner.json"
    )

    if not any(r["file"] == new_runner_path for r in runners) and not os.path.exists(
        new_runner_path
    ):

        new_runner = {
            "name": PROGRAM_NAME,
            "source_setup_command": [
                "python",
                "$WORK_DIR/script/unictest_setup.py",
                "setup",
                PROGRAM_NAME,
            ],
            "run_command": [
                "python",
                "$WORK_DIR/script/unictest_setup.py",
                "test",
                PROGRAM_NAME,
            ],
        }
        with open(actual_new_runner_path, "w") as f:
            json.dump(new_runner, f, indent=4)
        runners.append(
            {
                "file": new_runner_path,
            }
        )
        test_config["runners"] = runners
        with open(TEST_CONFIG_PATH, "w") as f:
            json.dump(test_config, f, indent=4)
        print(f"Added runner for ebm2{lang_name} to {TEST_CONFIG_PATH}")
    else:
        print(f"Runner for ebm2{lang_name} already exists in {TEST_CONFIG_PATH}")

    if not os.path.exists(PARENT_CMAKE_PATH):
        os.makedirs(os.path.dirname(PARENT_CMAKE_PATH), exist_ok=True)
        with open(PARENT_CMAKE_PATH, "w") as f:
            f.write("")

    with open(PARENT_CMAKE_PATH, "r") as f:
        content = f.read()

    if content.find(f"add_subdirectory(ebm2{lang_name})") != -1:
        print(
            f"Directory ebm2{lang_name} already exists in {PARENT_CMAKE_PATH}. Skipping update."
        )
    else:
        with open(PARENT_CMAKE_PATH, "w") as f:
            new_content = content + f"\nadd_subdirectory(ebm2{lang_name})\n"
            new_content = "\n".join(
                [x.strip() for x in new_content.splitlines() if x.strip() != ""]
            )
            f.write(new_content)
        print(f"Added subdirectory ebm2{lang_name} to {PARENT_CMAKE_PATH}")

    print("Code generation completed successfully.")
    print(f"Generated files are located in: {OUTPUT_DIR}")


def do_ebmgen_generate():
    TARGET_DIR = "src/ebmgen/visitor"
    os.makedirs(TARGET_DIR, exist_ok=True)
    VISITOR_HEADER = execute(
        [
            TOOL_PATH,
            "--mode",
            "ebmgen-visitor",
            "--program-name",
            "ebmgen::visitor",
        ],
        None,
    )
    TARRGET_PATH = os.path.join(TARGET_DIR, "visitor.hpp")
    with open(TARRGET_PATH, "rb") as f:
        existing_content = f.read() if os.path.exists(TARRGET_PATH) else None
        if existing_content is not None and existing_content == VISITOR_HEADER:
            print(f"Visitor header at {TARRGET_PATH} is already up to date.")
            return
    with open(TARRGET_PATH, "wb") as f:
        f.write(VISITOR_HEADER)
    print("Code generation for ebmgen visitor completed successfully.")
    print(f"Generated visitor header is located in: {TARRGET_PATH}")


def do_all(mode: str):
    # scan src/${mode} for all existing languages
    parent_dir = (
        f"src/ebmip"
        if (mode == "interpret" or mode == "interpret-class")
        else f"src/ebmcg"
    )
    do_default_dummy_header("all", mode)
    for entry in os.listdir(parent_dir):
        entry_path = os.path.join(parent_dir, entry)
        if os.path.isdir(entry_path) and entry.startswith("ebm2"):
            lang = entry[4:]  # remove "ebm2" prefix
            do_setup(lang, mode, "." + lang)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Generate code generator or interpreter."
    )
    parser.add_argument("lang_name", help="Language name")
    parser.add_argument(
        "--mode",
        choices=["codegen", "interpret", "codegen-class", "interpret-class"],
        default="codegen-class",
        help="Mode of operation",
    )
    parser.add_argument(
        "--file-extension",
        help="File extension for output source code",
    )
    args = parser.parse_args()

    lang_name = args.lang_name
    mode = args.mode
    file_extension = args.file_extension
    if file_extension is None:
        file_extension = "." + lang_name
    if lang_name == "all":
        for m in ["codegen-class", "interpret-class"]:
            do_all(m)
        do_ebmgen_generate()
    elif lang_name == "all-mode":
        do_all(mode)
    elif lang_name == "ebmgen":
        do_ebmgen_generate()
    else:
        do_setup(lang_name, mode, file_extension)
