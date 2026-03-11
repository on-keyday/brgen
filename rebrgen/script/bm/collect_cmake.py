import os
import pathlib as pl
import json
import sys

INPUT = "src/test/test_cases.bgn" if len(sys.argv) < 2 else sys.argv[1]
SRC2JSON = (
    "C:/workspace/shbrgen/brgen/tool/src2json" if len(sys.argv) < 3 else sys.argv[2]
)


def collect_cmake_files(root_dir):
    cmake_files = []
    for root, dirs, files in os.walk(root_dir):
        for file in files:
            if root == root_dir:
                continue
            if root == "src/old/bm2" or root == "src/old/bmgen":
                continue
            if file == "CMakeLists.txt":
                obj = {
                    "name": pl.Path(os.path.join(root)).relative_to(root_dir).as_posix()
                }
                obj["lang"] = obj["name"][3:]
                configPath = os.path.join(root, "config.json")
                obj["has_config"] = pl.Path(configPath).exists()
                if obj["has_config"]:
                    with open(configPath, "r") as f:
                        print(f"Loading config: {configPath}")
                        obj["config"] = json.load(f)
                        if obj["config"].get("suffix") is not None:
                            obj["suffix"] = obj["config"]["suffix"]
                        else:
                            obj["suffix"] = obj["lang"]
                else:
                    print("No config file found", obj["name"])
                    obj["suffix"] = obj["lang"]
                cmake_files.append(obj)
    return cmake_files


src = collect_cmake_files("src/old")

# print(json.dumps(src))
# exit(0)

output = "src/old/CMakeLists.txt"

with open(output, "w") as f:
    for file in src:
        f.write(f'add_subdirectory("{file["name"]}")\n')

output = "script/bm/run_generated.py"
with open(output, "w") as f:
    f.write("import os\n")
    f.write("import subprocess as sp\n")
    f.write("import sys\n")
    f.write("\n")
    f.write("sp.run(\n")
    f.write('    ["python","script/build.py"],\n')
    f.write("    check=True,\n")
    f.write("    stdout=sys.stdout,\n")
    f.write("    stderr=sys.stderr,\n")
    f.write(")\n")
    f.write(f'INPUT = "{INPUT}" if len(sys.argv) < 2 else sys.argv[1]\n')
    f.write('print(f"Input: {INPUT}")\n')
    f.write("save = sp.check_output(\n")
    f.write(f'    ["{SRC2JSON}", INPUT],\n')
    f.write("    stderr=sys.stderr,\n")
    f.write(")\n")
    f.write('with open("save/sample.json", "wb") as f:\n')
    f.write("    f.write(save)\n")
    f.write('print("Generated: save/sample.json")\n')
    f.write("save = sp.check_output(\n")
    f.write(
        '    ["tool/bmgen", "-p", "-i", "save/sample.json", "-o", "save/save.bin", "-c", "save/save.dot","--print-process-time"],\n'
    )
    f.write("    stderr=sys.stderr,\n")
    f.write(")\n")
    f.write('print("Generated: save/save.bin")\n')
    f.write('print("Generated: save/save.dot")\n')
    f.write('with open("save/save.txt", "wb") as f:\n')
    f.write("    f.write(save)\n")
    f.write('print("Generated: save/save.txt")\n')
    f.write("save = sp.check_output(\n")
    f.write(
        '    ["tool/bmgen", "-p", "-i", "save/sample.json", "--print-only-op","--print-process-time"],\n'
    )
    f.write("    stderr=sys.stderr,\n")
    f.write(")\n")
    f.write('with open("save/save_op.txt", "wb") as f:\n')
    f.write("    f.write(save)\n")
    f.write('print("Generated: save/save_op.txt")\n')
    f.write("\n")

    for file in src:
        if file["lang"] == "hexmap":
            continue  # skip this currently
        # Create directory for language output if it doesn't exist
        f.write(f'os.makedirs("save/{file["lang"]}", exist_ok=True)\n')
        f.write(f"\n")
        f.write(f"src = sp.check_output(\n")
        f.write(
            f'    ["tool/{file["name"]}", "-i", "save/save.bin","--test-info","save/{file["lang"]}/save.{file["suffix"]}.json"],\n'
        )
        f.write(f"    stderr=sys.stderr,\n")
        f.write(f")\n")
        # if dst file exists, first check if it is different
        f.write("cached = False\n")
        f.write(f'if os.path.exists("save/{file["lang"]}/save.{file["suffix"]}"):\n')
        f.write(
            f'    with open("save/{file["lang"]}/save.{file["suffix"]}", "rb") as f:\n'
        )
        f.write(f"        if f.read() == src:\n")
        f.write(
            f'            print(f"Cached: save/{file["lang"]}/save.{file["suffix"]}")\n'
        )
        f.write(f"            cached = True\n")
        f.write("if not cached:\n")
        f.write(
            f'    with open("save/{file["lang"]}/save.{file["suffix"]}", "wb") as f:\n'
        )
        f.write(f"        f.write(src)\n")
        f.write(f'    print(f"Generated: save/{file["lang"]}/save.{file["suffix"]}")\n')
script_root = "script/bm"

output = f"{script_root}/run_generated.bat"

with open(output, "w") as f:
    f.write("@echo off\n")
    f.write("setlocal\n")
    f.write(f"python {script_root}/run_generated.py %*\n")

output = f"{script_root}/run_generated.sh"

with open(output, "wb") as f:
    f.write("#!/bin/bash\n".encode())
    f.write(f'python {script_root}/run_generated.py "$@"\n'.encode())
output = f"{script_root}/run_generated.ps1"

with open(output, "w") as f:
    f.write("$ErrorActionPreference = 'Stop'\n")
    f.write(f"python {script_root}/run_generated.py @args\n")
