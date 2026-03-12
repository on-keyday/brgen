import subprocess as sp
import os
import sys
import shutil

LANG_LIST = ["c", "python", "haskell", "go", "cpp3", "kaitai"]
BUILD_MODE = "native"
BUILD_TYPE = "Debug"
GOLDEN_MASTER_DIR = "test/golden_masters"


from pathlib import Path

sys.path.append(str(Path(__file__).parent))
from util import execute


def build_gen_template():
    print("Building gen_template...")
    execute(
        [
            "cmake",
            "--build",
            f"./built/{BUILD_MODE}/{BUILD_TYPE}",
            "--target",
            "gen_template",
        ],
        capture=False,
    )
    execute(
        [
            "cmake",
            "--install",
            f"./built/{BUILD_MODE}/{BUILD_TYPE}",
            "--component",
            "gen_template",
        ],
        capture=False,
    )
    print("gen_template built successfully.")


def generate_golden_masters():
    if os.path.exists(GOLDEN_MASTER_DIR):
        shutil.rmtree(GOLDEN_MASTER_DIR)
    os.makedirs(GOLDEN_MASTER_DIR)
    print(f"Created golden master directory: {GOLDEN_MASTER_DIR}")

    for lang in LANG_LIST:
        lang_output_dir = os.path.join(GOLDEN_MASTER_DIR, lang)
        os.makedirs(lang_output_dir, exist_ok=True)
        print(f"Generating golden masters for language: {lang}")

        # Generate .hpp
        header_content = execute(
            [
                "./tool/gen_template",
                "--lang",
                lang,
                "--mode",
                "header",
                "--hook-dir",
                f"src/old/bm2{lang}/hook",
            ]
        )
        with open(os.path.join(lang_output_dir, f"bm2{lang}.hpp"), "wb") as f:
            f.write(header_content)

        # Generate .cpp
        cpp_content = execute(
            [
                "./tool/gen_template",
                "--mode",
                "generator",
                "--config-file",
                f"src/old/bm2{lang}/config.json",
                "--hook-dir",
                f"src/old/bm2{lang}/hook",
            ]
        )
        with open(os.path.join(lang_output_dir, f"bm2{lang}.cpp"), "wb") as f:
            f.write(cpp_content)

        # Generate main.cpp
        main_content = execute(
            [
                "./tool/gen_template",
                "--lang",
                lang,
                "--mode",
                "main",
                "--hook-dir",
                f"src/old/bm2{lang}/hook",
            ]
        )
        with open(os.path.join(lang_output_dir, f"main.cpp"), "wb") as f:
            f.write(main_content)

        # Generate CMakeLists.txt
        cmake_content = execute(
            [
                "./tool/gen_template",
                "--lang",
                lang,
                "--mode",
                "cmake",
                "--hook-dir",
                f"src/old/bm2{lang}/hook",
            ]
        )
        with open(os.path.join(lang_output_dir, f"CMakeLists.txt"), "wb") as f:
            f.write(cmake_content)

    print("Golden master generation complete.")


if __name__ == "__main__":
    build_gen_template()
    generate_golden_masters()
