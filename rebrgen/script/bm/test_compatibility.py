import subprocess as sp
import os
import sys
import shutil
import filecmp

LANG_LIST = ["c", "python", "haskell", "go", "cpp3", "kaitai"]
BUILD_MODE = "native"
BUILD_TYPE = "Debug"
GOLDEN_MASTER_DIR = "test/golden_masters"
TEMP_GEN_DIR = "test/temp_generated"


from ..util import execute


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


def test_compatibility():
    if os.path.exists(TEMP_GEN_DIR):
        shutil.rmtree(TEMP_GEN_DIR)
    os.makedirs(TEMP_GEN_DIR)
    print(f"Created temporary generation directory: {TEMP_GEN_DIR}")

    all_tests_passed = True

    for lang in LANG_LIST:
        print(f"Testing compatibility for language: {lang}")
        lang_temp_dir = os.path.join(TEMP_GEN_DIR, lang)
        os.makedirs(lang_temp_dir, exist_ok=True)

        # Define files to generate and compare
        files_to_generate = {
            f"bm2{lang}.hpp": [
                "./tool/gen_template",
                "--lang",
                lang,
                "--mode",
                "header",
                "--hook-dir",
                f"src/old/bm2{lang}/hook",
            ],
            f"bm2{lang}.cpp": [
                "./tool/gen_template",
                "--mode",
                "generator",
                "--config-file",
                f"src/old/bm2{lang}/config.json",
                "--hook-dir",
                f"src/old/bm2{lang}/hook",
            ],
            f"main.cpp": [
                "./tool/gen_template",
                "--lang",
                lang,
                "--mode",
                "main",
                "--hook-dir",
                f"src/old/bm2{lang}/hook",
            ],
            f"CMakeLists.txt": [
                "./tool/gen_template",
                "--lang",
                lang,
                "--mode",
                "cmake",
                "--hook-dir",
                f"src/old/bm2{lang}/hook",
            ],
        }

        for filename, command_args in files_to_generate.items():
            temp_file_path = os.path.join(lang_temp_dir, filename)
            golden_file_path = os.path.join(GOLDEN_MASTER_DIR, lang, filename)

            try:
                generated_content = execute(command_args)
                with open(temp_file_path, "wb") as f:
                    f.write(generated_content)

                if not os.path.exists(golden_file_path):
                    print(f"ERROR: Golden master file not found: {golden_file_path}")
                    all_tests_passed = False
                    continue

                if filecmp.cmp(temp_file_path, golden_file_path, shallow=False):
                    print(f"  {filename}: PASSED")
                else:
                    print(f"  {filename}: FAILED (Differences found)")
                    all_tests_passed = False

            except sp.CalledProcessError as e:
                print(f"ERROR: Command failed for {filename} in {lang}: {e}")
                all_tests_passed = False
            except Exception as e:
                print(
                    f"ERROR: An unexpected error occurred for {filename} in {lang}: {e}"
                )
                all_tests_passed = False

    shutil.rmtree(TEMP_GEN_DIR)
    print(f"Cleaned up temporary generation directory: {TEMP_GEN_DIR}")

    if all_tests_passed:
        print("All compatibility tests PASSED!")
    else:
        print("Some compatibility tests FAILED.")
        sys.exit(1)


if __name__ == "__main__":
    build_gen_template()
    test_compatibility()
