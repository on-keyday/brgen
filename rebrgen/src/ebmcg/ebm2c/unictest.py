#!/usr/bin/env python3
# Test logic for ebm2c
import sys
import os
import subprocess
import pathlib
import json


def main():
    TEST_TARGET_FILE = sys.argv[1]  # The generated .c file
    INPUT_FILE = sys.argv[2]
    OUTPUT_FILE = sys.argv[3]
    TEST_TARGET_FORMAT = sys.argv[4]  # The struct name

    print(f"Testing {TEST_TARGET_FILE} with {INPUT_FILE} and {OUTPUT_FILE}")

    proj_dir = pathlib.Path("c_proj")
    os.makedirs(proj_dir, exist_ok=True)

    # Copy generated C code
    generated_c_file = pathlib.Path(TEST_TARGET_FILE)

    # We copy the content to a file named 'generated.h' to treat it as a single-header library
    # as per instructions "assume single header".
    generated_h_name = "generated.h"
    with open(generated_c_file, "r") as f_src:
        with open(proj_dir / generated_h_name, "w") as f_dst:
            f_dst.write(f_src.read())

    # Create CMakeLists.txt
    with open(proj_dir / "CMakeLists.txt", "w") as f:
        f.write("cmake_minimum_required(VERSION 3.10)\n")
        f.write(f"project(test_runner C)\n\n")
        # We only compile main.c, generated.h is included
        f.write(f"add_executable(test_runner main.c)\n")
        f.write(
            "target_include_directories(test_runner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})\n"
        )
        # Disable some warnings if necessary, as generated code might trigger them
        if os.name != "nt":
            f.write(
                "target_compile_options(test_runner PRIVATE -Wall -Wextra -Wno-unused-variable -Wno-unused-function)\n"
            )

        # Create main.c

        with open(proj_dir / "main.c", "w") as f:

            f.write(
                f"""
    #include <stdio.h>
    #include <stdlib.h> 
    #include <string.h>
    #include <stdbool.h>

    #define EBM_FUNCTION_PROLOGUE() do {{\\
      printf("Function prologue %s %llx\\n",__func__,input->offset);\\
    }} while(0)

    #define EBM_FUNCTION_EPILOGUE() do {{\\
        printf("Function epilogue %s %llx\\n",__func__,input->offset);\\
    }} while(0)

    #include "{generated_h_name}"

    

    /* 
       Assumptions based on ebm2c generation:

       1. Struct name is {TEST_TARGET_FORMAT}

       2. Decode function: int {TEST_TARGET_FORMAT}_decode({TEST_TARGET_FORMAT}* self, DecoderInput* input);

       3. Encode function: int {TEST_TARGET_FORMAT}_encode({TEST_TARGET_FORMAT}* self, EncoderInput* input);

       4. DecoderInput and EncoderInput structs are defined in generated code.
    */

    void default_set_last_error(const char* msg) {{
        fprintf(stderr, "EBM Error: %s\\n", msg);
    }}

    #ifdef LAST_ERROR_HANDLER
    void* default_allocate(struct DecoderInput* self, size_t size) {{
        (void)self;
        return malloc(size);
    }}

    /* Assumes vector structure matches VECTOR_OF(void) */
    #ifdef VECTOR_OF
    typedef struct {{
        void* data;
        size_t size;
        size_t capacity;
    }} GenericVector;

    int default_append(struct DecoderInput* self, VECTOR_OF(void)* vector_void, const void* elem_data, size_t elem_size,const char* type_str) {{
        (void)self;
        GenericVector* vector = (GenericVector*)vector_void;
        if (vector->size >= vector->capacity) {{
            size_t new_capacity = vector->capacity == 0 ? 4 : vector->capacity * 2;
            void* new_data = realloc(vector->data, new_capacity * elem_size);
            if (!new_data) return -1;
            vector->data = new_data;
            vector->capacity = new_capacity;
        }}
        memcpy((char*)vector->data + (vector->size * elem_size), elem_data, elem_size);
        vector->size++;
        return 0;
    }}
    #endif
    #endif

    int main(int argc, char *argv[]) {{

        if (argc < 3) {{

            fprintf(stderr, "Usage: %s <input_file> <output_file>\\n", argv[0]);

            return 1;

        }}

    

        const char* input_path = argv[1];

        const char* output_path = argv[2];

    

        /* Read input file */

        FILE* fp_in = fopen(input_path, "rb");

        if (!fp_in) {{

            fprintf(stderr, "Failed to open input file '%s'\\n", input_path);

            return 1;

        }}

    

        fseek(fp_in, 0, SEEK_END);

        long input_len = ftell(fp_in);

        fseek(fp_in, 0, SEEK_SET);

    

        uint8_t* input_buffer = (uint8_t*)malloc(input_len);

        if (!input_buffer) {{

            fprintf(stderr, "Failed to allocate memory for input buffer\\n");

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

    

        /* Setup Decoder */

        DecoderInput decoder_input;

        memset(&decoder_input, 0, sizeof(decoder_input));

        decoder_input.data = input_buffer;

        decoder_input.data_end = input_buffer + input_len;

        decoder_input.offset = 0;
        #ifdef LAST_ERROR_HANDLER
        decoder_input.set_last_error = default_set_last_error;
        #endif
        #ifdef EBM_ALLOCATE
        decoder_input.allocate = default_allocate;
        #endif
        #ifdef VECTOR_OF
        decoder_input.append = default_append;
        #endif
        /* decoder_input.can_read = NULL; */ /* Use default logic */

        

        /* Decode */

        {TEST_TARGET_FORMAT} target_obj;

        memset(&target_obj, 0, sizeof(target_obj)); // Initialize struct

        

        /* 

           Note: We assume the standard naming pattern <Struct>_decode 

           and that it takes self pointer first.

        */

        int decode_res = {TEST_TARGET_FORMAT}_decode(&target_obj, &decoder_input);

        

        if (decode_res != 0) {{

            fprintf(stderr, "Decode failed with code %d\\n", decode_res);

            free(input_buffer);

            return 10;

        }}

    

        /* Setup Encoder */

        size_t output_capacity = input_len * 2; /* Start with double input size */

        if (output_capacity < 1024) output_capacity = 1024;

        

        uint8_t* output_buffer = (uint8_t*)malloc(output_capacity);

        if (!output_buffer) {{

             fprintf(stderr, "Failed to allocate memory for output buffer\\n");

             free(input_buffer);

             return 1;

        }}

    

        EncoderInput encoder_input;

        memset(&encoder_input, 0, sizeof(encoder_input));

        encoder_input.data = output_buffer;

        encoder_input.data_end = output_buffer + output_capacity;

        encoder_input.offset = 0;

        #ifdef VECTOR_OF
        /* encoder_input.emit = NULL; */ /* Use fixed buffer for now, simplest for test */
        #endif
        #ifdef LAST_ERROR_HANDLER
        encoder_input.set_last_error = default_set_last_error;
        #endif

    

        /* Encode */

        int encode_res = {TEST_TARGET_FORMAT}_encode(&target_obj, &encoder_input);

        

        if (encode_res != 0) {{

            fprintf(stderr, "Encode failed with code %d\\n", encode_res);

            free(input_buffer);

            free(output_buffer);

            return 20;

        }}

    

        /* Write output file */

        FILE* fp_out = fopen(output_path, "wb");

        if (!fp_out) {{

            fprintf(stderr, "Failed to open output file '%s'\\n", output_path);

            free(input_buffer);

            free(output_buffer);

            return 1;

        }}

        fwrite(output_buffer, 1, encoder_input.offset, fp_out);

        fclose(fp_out);

    

        free(input_buffer);

        free(output_buffer);

        

        /* TODO: We might need a way to free 'target_obj' if it contains allocated memory (vectors/arrays) 

           but currently there is no standard free function in the generated code I saw. 

           This will leak memory in the test runner but OS cleans up on exit. */


        return 0;
    }}
    """
            )

    # Configure and build the C project
    print("\nConfiguring C project with CMake...")
    build_dir = proj_dir / "build"
    os.makedirs(build_dir, exist_ok=True)

    cmake_command = [
        "cmake",
        "-S",
        "..",
        "-B",
        ".",
        "-G",
        "Ninja",
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DCMAKE_C_COMPILER=clang",
    ]
    if os.name != "nt":
        cmake_command.append("-DCMAKE_C_FLAGS=-fsanitize=address")

    result = subprocess.run(
        cmake_command,
        cwd=build_dir,
        stderr=sys.stderr,
        stdout=sys.stdout,
        text=True,
    )
    if result.returncode != 0:
        print("CMake configuration failed!")
        print(result.stdout)
        print(result.stderr)
        sys.exit(1)

    print("Building C project...")
    build_command = ["cmake", "--build", ".", "--config", "Debug"]

    result = subprocess.run(
        build_command,
        cwd=build_dir,
        stderr=sys.stderr,
        stdout=sys.stdout,
        text=True,
    )
    if result.returncode != 0:
        print("C compilation failed!")
        print(result.stdout)
        print(result.stderr)
        sys.exit(1)

    print("Compilation successful.")

    # Run the compiled test executable
    executable_name = "test_runner.exe" if os.name == "nt" else "test_runner"
    executable_path = build_dir / executable_name

    executable_path = executable_path.resolve()

    print("for VSCode debugging")
    print(
        json.dumps(
            {
                "type": "cppvsdbg" if os.name == "nt" else "cppdbg",
                "request": "launch",
                "cwd": os.getcwd(),
                "name": f"Debug ebm2c unictest ({TEST_TARGET_FORMAT})",
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
        stdout=sys.stdout,
        stderr=sys.stderr,
    )

    if proc.returncode != 0:
        print(f"Test executable failed with exit code {proc.returncode}")

    sys.exit(proc.returncode)


if __name__ == "__main__":
    main()
