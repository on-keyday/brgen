#!/usr/bin/env python3
# Test logic for ebm2cpp
import sys
import os
import subprocess
import pathlib
import json


def main():
    TEST_TARGET_FILE = sys.argv[1]  # The generated .cpp file
    INPUT_FILE = sys.argv[2]
    OUTPUT_FILE = sys.argv[3]
    TEST_TARGET_FORMAT = sys.argv[4]  # The struct name
    OPTION_SET_NAME = sys.argv[5]
    ADDITIONAL_ARGS = sys.argv[6:] if len(sys.argv) > 6 else []

    print(f"Testing {TEST_TARGET_FILE} with {INPUT_FILE} and {OUTPUT_FILE}")

    proj_dir = pathlib.Path("cpp_proj")
    os.makedirs(proj_dir, exist_ok=True)

    # Copy generated C++ code as a header
    generated_cpp_file = pathlib.Path(TEST_TARGET_FILE)
    generated_h_name = "generated.hpp"
    with open(generated_cpp_file, "r") as f_src:
        with open(proj_dir / generated_h_name, "w") as f_dst:
            f_dst.write(f_src.read())

    # Find futils include path
    original_work_dir = os.environ.get("UNICTEST_ORIGINAL_WORK_DIR", "")
    # Try build_config.json for FUTILS_DIR
    futils_dir = ""
    build_config_path = os.path.join(original_work_dir, "build_config.json")
    if os.path.exists(build_config_path):
        with open(build_config_path) as f:
            build_config = json.load(f)
            futils_dir = build_config.get("FUTILS_DIR", "")

    if not futils_dir:
        # Fallback: look for utils relative to brgen
        futils_dir = os.path.join(original_work_dir, "..", "..", "utils_backup")

    # Resolve against original_work_dir when relative, since this script runs in
    # a temp work dir where the relative path no longer points at futils.
    if not os.path.isabs(futils_dir) and original_work_dir:
        futils_dir = os.path.join(original_work_dir, futils_dir)
    futils_dir = os.path.abspath(futils_dir)

    futils_include = os.path.join(futils_dir, "src", "include")
    if not os.path.isdir(futils_include):
        futils_include = os.path.join(futils_dir, "include")

    # Create CMakeLists.txt
    futils_include_escaped = futils_include.replace("\\", "/")
    with open(proj_dir / "CMakeLists.txt", "w") as f:
        f.write("cmake_minimum_required(VERSION 3.20)\n")
        f.write("project(test_runner CXX)\n\n")
        f.write("set(CMAKE_CXX_STANDARD 20)\n")
        f.write("set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n")
        f.write("add_executable(test_runner main.cpp)\n")
        f.write(f'target_include_directories(test_runner PRIVATE "${{CMAKE_CURRENT_SOURCE_DIR}}" "{futils_include_escaped}")\n')
        if os.name != "nt":
            f.write('target_compile_options(test_runner PRIVATE -Wall -Wextra -Wno-unused-variable -Wno-unused-function)\n')

    # Create main.cpp test harness
    with open(proj_dir / "main.cpp", "w") as f:
        f.write(f"""
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "{generated_h_name}"

int main(int argc, char* argv[]) {{
    if (argc < 3) {{
        fprintf(stderr, "Usage: %s <input_file> <output_file>\\n", argv[0]);
        return 1;
    }}

    const char* input_path = argv[1];
    const char* output_path = argv[2];

    // Read input file
    FILE* fp_in = fopen(input_path, "rb");
    if (!fp_in) {{
        fprintf(stderr, "Failed to open input file '%s'\\n", input_path);
        return 1;
    }}

    fseek(fp_in, 0, SEEK_END);
    long input_len = ftell(fp_in);
    fseek(fp_in, 0, SEEK_SET);

    std::vector<std::uint8_t> input_buffer(input_len);
    if (fread(input_buffer.data(), 1, input_len, fp_in) != (size_t)input_len) {{
        fprintf(stderr, "Failed to read input file\\n");
        fclose(fp_in);
        return 1;
    }}
    fclose(fp_in);

    // Decode
    {TEST_TARGET_FORMAT} target_obj{{}};
    ::futils::binary::reader r{{::futils::view::rvec(input_buffer.data(), input_len)}};
    auto decode_err = target_obj.decode(r);
    if (decode_err) {{
        fprintf(stderr, "Decode failed: %s\\n", decode_err.error<std::string>().c_str());
        return 10;
    }}

    // Encode
    std::vector<std::uint8_t> output_buffer;
    output_buffer.resize(input_len * 2 > 1024 ? input_len * 2 : 1024);
    ::futils::binary::writer w{{::futils::view::wvec(output_buffer.data(), output_buffer.size())}};
    auto encode_err = target_obj.encode(w);
    if (encode_err) {{
        fprintf(stderr, "Encode failed: %s\\n", encode_err.error<std::string>().c_str());
        return 20;
    }}

    // Write output file
    FILE* fp_out = fopen(output_path, "wb");
    if (!fp_out) {{
        fprintf(stderr, "Failed to open output file '%s'\\n", output_path);
        return 1;
    }}
    fwrite(output_buffer.data(), 1, w.offset(), fp_out);
    fclose(fp_out);

    return 0;
}}
""")

    # Configure and build
    print("\nConfiguring C++ project with CMake...")
    build_dir = proj_dir / "build"
    os.makedirs(build_dir, exist_ok=True)

    # Find clang++ path - on Windows, use full path for CMake/Ninja compatibility
    import shutil
    clangpp = shutil.which("clang++")
    if clangpp:
        clangpp = clangpp.replace("\\", "/")
    else:
        clangpp = "clang++"

    cmake_command = [
        "cmake",
        "-S", "..",
        "-B", ".",
        "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Debug",
        f"-DCMAKE_CXX_COMPILER={clangpp}",
    ]
    if os.name != "nt":
        cmake_command.append("-DCMAKE_CXX_FLAGS=-fsanitize=address")

    result = subprocess.run(
        cmake_command,
        cwd=build_dir,
        stderr=sys.stderr,
        stdout=sys.stdout,
        text=True,
    )
    if result.returncode != 0:
        print("CMake configuration failed!")
        sys.exit(1)

    print("Building C++ project...")
    build_command = ["cmake", "--build", ".", "--config", "Debug"]
    result = subprocess.run(
        build_command,
        cwd=build_dir,
        stderr=sys.stderr,
        stdout=sys.stdout,
        text=True,
    )
    if result.returncode != 0:
        print("C++ compilation failed!")
        sys.exit(1)

    print("Compilation successful.")

    # Run the compiled test executable
    executable_name = "test_runner.exe" if os.name == "nt" else "test_runner"
    executable_path = (build_dir / executable_name).resolve()

    print("for VSCode debugging")
    print(
        json.dumps(
            {
                "type": "cppvsdbg" if os.name == "nt" else "cppdbg",
                "request": "launch",
                "cwd": os.getcwd(),
                "name": f"Debug ebm2cpp unictest ({TEST_TARGET_FORMAT})",
                "program": executable_path.as_posix(),
                "args": [INPUT_FILE, OUTPUT_FILE],
                "stopAtEntry": True,
            },
            indent=4,
        )
    )

    print(f"\nRunning compiled test: {executable_path.as_posix()}")
    proc = subprocess.run(
        [executable_path.as_posix(), INPUT_FILE, OUTPUT_FILE],
        stdout=sys.stdout,
        stderr=sys.stderr,
    )

    if proc.returncode != 0:
        print(f"Test executable failed with exit code {proc.returncode}")

    sys.exit(proc.returncode)


if __name__ == "__main__":
    main()
