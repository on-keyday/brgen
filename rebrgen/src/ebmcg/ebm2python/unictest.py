#!/usr/bin/env python3
# Test logic for ebm2python
import sys
import os
import subprocess as sp
import json


def main():
    TEST_TARGET_FILE = sys.argv[1]
    INPUT_FILE = sys.argv[2]
    OUTPUT_FILE = sys.argv[3]
    TEST_TARGET_FORMAT = sys.argv[4]
    # Test logic goes here
    print(f"Testing {TEST_TARGET_FILE} with {INPUT_FILE} and {OUTPUT_FILE}")
    # Make stub python module that
    os.makedirs("ebm_python_module", exist_ok=True)
    with open(f"ebm_python_module/__init__.py", "w") as f:
        f.write(f"from .test_target import {TEST_TARGET_FORMAT}\n")
    # copy test_target to stub module
    with open(TEST_TARGET_FILE, "rb") as f_src:
        with open(
            os.path.join("ebm_python_module", os.path.basename(TEST_TARGET_FILE)), "wb"
        ) as f_dst:
            f_dst.write(f_src.read())
    print(f"Created stub module in ebm_python_module/")
    # add test script file that uses the module
    with open("test_script.py", "w") as f:
        f.write("#!/usr/bin/env python3\n")
        f.write("import sys\n")
        f.write("import io\n")
        f.write("import traceback\n")
        f.write(f"from ebm_python_module import {TEST_TARGET_FORMAT}\n")
        f.write("def main():\n")
        f.write(f"    print('Running test for {TEST_TARGET_FORMAT}')\n")
        f.write("    with open(sys.argv[1], 'rb') as f:\n")
        f.write("       data = f.read()\n")
        f.write("       data = io.BytesIO(data)\n")
        f.write(f"    target = {TEST_TARGET_FORMAT}()\n")
        f.write("    try:\n")
        f.write("       target.decode(data)\n")
        f.write("    except Exception as e:\n")
        f.write("       print(f'Error during decoding: {e}')\n")
        f.write("       print(traceback.format_exc())\n")
        f.write("       sys.exit(10) # for testing purposes\n")
        f.write("    decoded_data = f'{target}'\n")
        if os.environ["UNICTEST_VERBOSE"] == "0":
            f.write(
                "    decoded_data = decoded_data[0:100] + '...' if len(decoded_data) > 100 else decoded_data\n"
            )
        f.write("    print(f'Decoded data: {decoded_data}')\n")
        f.write("    data = io.BytesIO()\n")
        f.write("    try:\n")
        f.write("       target.encode(data)\n")
        f.write("    except Exception as e:\n")
        f.write("       print(f'Error during encoding: {e}')\n")
        f.write("       print(traceback.format_exc())\n")
        f.write("       sys.exit(20) # for testing purposes\n")
        f.write("    with open(sys.argv[2], 'wb') as f:\n")
        f.write("        f.write(data.getvalue())\n")
        f.write("    print('Test completed successfully.')\n")
        f.write("if __name__ == '__main__':\n")
        f.write("    main()\n")
    # Run the test script
    print(f"\nRunning test script: test_script.py", flush=True)
    print("for VSCode debugging")
    print(
        json.dumps(
            {
                "type": "debugpy",
                "request": "launch",
                "cwd": os.getcwd(),
                "name": f"Debug ebm2python unictest ({TEST_TARGET_FORMAT})",
                "program": os.path.abspath("test_script.py"),
                "args": [
                    INPUT_FILE,
                    OUTPUT_FILE,
                ],
            },
            indent=4,
        )
    )
    try:
        sp.check_call(
            [
                sys.executable,
                "test_script.py",
                INPUT_FILE,
                OUTPUT_FILE,
            ],
            env=os.environ,
            stdout=sys.stdout,
            stderr=sys.stderr,
        )
    except sp.CalledProcessError as e:
        sys.exit(e.returncode)


if __name__ == "__main__":
    main()
