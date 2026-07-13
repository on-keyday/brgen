#!/usr/bin/env python3
# Test logic for ebm2llvm: compile the generated LLVM IR together with a C
# driver using clang, then run a decode -> encode round-trip.
#
# ABI contract with the generated IR (see visitor/entry_before_class.hpp):
#   %Vec    = type { ptr, i64, i64 }  ; data, size, capacity
#   %Stream = type { ptr, i64, i64 }  ; data, len(dec)/cap(enc), offset
#   define i32 @<Format>_decode(ptr %self, ptr %stream)  ; 0 = ok, nonzero = error
#   define i32 @<Format>_encode(ptr %self, ptr %stream)
import sys
import os
import subprocess
import pathlib
import json

import unictest_report

MAIN_C_TEMPLATE = """
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* matches %Stream = type {{ ptr, i64, i64 }} */
typedef struct {{
    uint8_t* data;
    size_t limit; /* len for decode, capacity for encode */
    size_t offset;
}} Stream;

extern int {format}_decode(void* self, Stream* in);
extern int {format}_encode(void* self, Stream* out);

/* The struct size is unknown on the C side; use a large zeroed arena. */
static _Alignas(16) unsigned char self_storage[1 << 20];

int main(int argc, char* argv[]) {{
    if (argc < 3) {{
        fprintf(stderr, "Usage: %s <input_file> <output_file>\\n", argv[0]);
        return 1;
    }}

    FILE* fp_in = fopen(argv[1], "rb");
    if (!fp_in) {{
        fprintf(stderr, "Failed to open input file '%s'\\n", argv[1]);
        return 1;
    }}
    fseek(fp_in, 0, SEEK_END);
    long input_len = ftell(fp_in);
    fseek(fp_in, 0, SEEK_SET);
    uint8_t* input_buffer = (uint8_t*)malloc(input_len ? input_len : 1);
    if (!input_buffer) {{
        fclose(fp_in);
        return 1;
    }}
    if (fread(input_buffer, 1, input_len, fp_in) != (size_t)input_len) {{
        fprintf(stderr, "Failed to read input file\\n");
        free(input_buffer);
        fclose(fp_in);
        return 1;
    }}
    fclose(fp_in);

    Stream dec = {{ input_buffer, (size_t)input_len, 0 }};
    int decode_res = {format}_decode(self_storage, &dec);
    if (decode_res != 0) {{
        fprintf(stderr, "Decode failed with code %d\\n", decode_res);
        free(input_buffer);
        return 10;
    }}

    size_t output_capacity = (size_t)input_len * 2;
    if (output_capacity < 1024) {{
        output_capacity = 1024;
    }}
    uint8_t* output_buffer = (uint8_t*)malloc(output_capacity);
    if (!output_buffer) {{
        free(input_buffer);
        return 1;
    }}
    Stream enc = {{ output_buffer, output_capacity, 0 }};
    int encode_res = {format}_encode(self_storage, &enc);
    if (encode_res != 0) {{
        fprintf(stderr, "Encode failed with code %d\\n", encode_res);
        free(input_buffer);
        free(output_buffer);
        return 20;
    }}

    FILE* fp_out = fopen(argv[2], "wb");
    if (!fp_out) {{
        fprintf(stderr, "Failed to open output file '%s'\\n", argv[2]);
        free(input_buffer);
        free(output_buffer);
        return 1;
    }}
    fwrite(output_buffer, 1, enc.offset, fp_out);
    fclose(fp_out);

    free(input_buffer);
    free(output_buffer);
    return 0;
}}
"""


def main():
    TEST_TARGET_FILE = sys.argv[1]  # The generated .ll file
    INPUT_FILE = sys.argv[2]
    OUTPUT_FILE = sys.argv[3]
    TEST_TARGET_FORMAT = sys.argv[4]

    print(f"Testing {TEST_TARGET_FILE} with {INPUT_FILE} and {OUTPUT_FILE}")

    # Refuse to link IR that still contains unimplemented-hook markers;
    # clang errors on them anyway but this gives a precise reason.
    with open(TEST_TARGET_FILE, "r", encoding="utf-8", errors="replace") as f:
        generated_ir = f.read()
    if "{{Unimplemented" in generated_ir:
        markers = sorted(
            set(
                line.strip()
                for line in generated_ir.splitlines()
                if "{{Unimplemented" in line
            )
        )
        for m in markers[:10]:
            print(f"unimplemented marker: {m}")
        unictest_report.fail(
            "compile", f"generated IR contains {len(markers)} unimplemented markers"
        )

    proj_dir = pathlib.Path("llvm_proj")
    os.makedirs(proj_dir, exist_ok=True)

    main_c = proj_dir / "main.c"
    with open(main_c, "w") as f:
        f.write(MAIN_C_TEMPLATE.format(format=TEST_TARGET_FORMAT))

    executable_name = "test_runner.exe" if os.name == "nt" else "test_runner"
    executable_path = (proj_dir / executable_name).resolve()

    compile_cmd = [
        "clang",
        "-g",
        "-Wno-override-module",
        main_c.as_posix(),
        pathlib.Path(TEST_TARGET_FILE).as_posix(),
        "-o",
        executable_path.as_posix(),
    ]
    if os.name != "nt":
        compile_cmd.insert(1, "-fsanitize=address")

    print(f"\nCompiling: {' '.join(compile_cmd)}")
    result = subprocess.run(compile_cmd, capture_output=True, text=True)
    sys.stdout.write(result.stdout or "")
    sys.stderr.write(result.stderr or "")
    if result.returncode != 0:
        print("clang compilation failed!")
        unictest_report.fail("compile", (result.stdout or "") + (result.stderr or ""))

    print("Compilation successful.")

    print("for VSCode debugging")
    print(
        json.dumps(
            {
                "type": "cppvsdbg" if os.name == "nt" else "cppdbg",
                "request": "launch",
                "cwd": os.getcwd(),
                "name": f"Debug ebm2llvm unictest ({TEST_TARGET_FORMAT})",
                "program": executable_path.as_posix(),
                "args": [INPUT_FILE, OUTPUT_FILE],
                "stopAtEntry": True,
            },
            indent=4,
        )
    )

    print(f"\nRunning compiled test: {executable_path.as_posix()}")
    # vector storage is realloc'd and never freed (no destructor generation
    # yet): disable LeakSanitizer, keep the memory-error detection
    run_env = os.environ.copy()
    run_env["ASAN_OPTIONS"] = (
        run_env.get("ASAN_OPTIONS", "") + ":detect_leaks=0"
    ).lstrip(":")
    proc = subprocess.run(
        [executable_path.as_posix(), INPUT_FILE, OUTPUT_FILE],
        capture_output=True,
        text=True,
        env=run_env,
    )
    sys.stdout.write(proc.stdout or "")
    sys.stderr.write(proc.stderr or "")

    if proc.returncode != 0:
        print(f"Test executable failed with exit code {proc.returncode}")
        phase = {10: "decode", 20: "encode"}.get(proc.returncode, "run")
        unictest_report.fail(phase, proc.stderr, code=proc.returncode)

    sys.exit(proc.returncode)


if __name__ == "__main__":
    main()
