import os
import sys
import pathlib as pl
import subprocess as sp
import json
import difflib

mode = sys.argv[1]
target_command = sys.argv[2]
preferred_file_ext = sys.argv[3] if len(sys.argv) > 3 else None

env = os.environ.copy()

unictest_env_vars = {k: v for k, v in env.items() if k.startswith("UNICTEST_")}

fuzz_mode = unictest_env_vars.get("UNICTEST_FUZZ_MODE", "0") == "1"
fuzz_seed = unictest_env_vars.get("UNICTEST_FUZZ_SEED", "0")

for e in list(unictest_env_vars.keys()):
    print(f"{e}={unictest_env_vars[e]}")

runner_dir = unictest_env_vars["UNICTEST_RUNNER_DIR"]
original_workdir = unictest_env_vars["UNICTEST_ORIGINAL_WORK_DIR"]
ebm2target = (pl.Path(original_workdir) / f"tool/{target_command}").as_posix()
if os.name == "nt":
    ebm2target += ".exe"


ebm2target_spec = json.loads(sp.check_output([ebm2target, "--show-flags"]).decode())
file_exts = ebm2target_spec.get("file_extensions", [])
if preferred_file_ext is None:
    if len(file_exts) > 0:
        file_ext = file_exts[0]
    else:
        print(f"No file extensions specified by target command: {target_command}")
else:
    file_ext = preferred_file_ext


setup_target_file = pl.Path(runner_dir) / f"test_target{file_ext}"

# Resolve which source directory the tool lives in (ebmcg = compiled generator, ebmip = interpreter)
def find_tool_unictest_script(workdir: pl.Path, command: str) -> pl.Path:
    for subdir in ("ebmcg", "ebmip"):
        candidate = workdir / "src" / subdir / command / "unictest.py"
        if candidate.exists():
            return candidate
    raise FileNotFoundError(f"unictest.py not found for {command} in src/ebmcg or src/ebmip")

def is_interpreter_tool(workdir: pl.Path, command: str) -> bool:
    return (workdir / "src" / "ebmip" / command / "unictest.py").exists()


def hexdump(data: bytes) -> str:
    result = ""
    for i in range(0, len(data), 16):
        chunk = data[i : i + 16]
        hex_bytes = " ".join(f"{b:02x}" for b in chunk)
        ascii_bytes = "".join((chr(b) if 32 <= b < 127 else ".") for b in chunk)
        result += f"{i:08x}  {hex_bytes:<48}  |{ascii_bytes}|\n"
    return result


if mode == "setup":

    ebmgen = (pl.Path(original_workdir) / "tool/ebmgen").as_posix()
    if os.name == "nt":
        ebmgen += ".exe"
    ebm_input_file = (pl.Path(runner_dir) / "runner_input.ebm").as_posix()

    cmd = [
        ebmgen,
        "-i",
        unictest_env_vars["UNICTEST_SOURCE_FILE"],
        "-o",
        ebm_input_file,
    ]
    print(f"\nRunning command: {' '.join(cmd)}")
    sp.check_call(cmd, timeout=60)

    additional_args = unictest_env_vars["UNICTEST_OPTION_SET_SETUP_OPTIONS"].split(",")

    if is_interpreter_tool(pl.Path(original_workdir), target_command):
        # Interpreter tools (ebmip) don't generate source code.
        # Write the EBM file path so unictest.py can locate it.
        with open(setup_target_file, "w") as f:
            f.write(ebm_input_file)
        print(f"Interpreter tool setup: EBM path written to {setup_target_file}")
    else:
        test_info_file = pl.Path(runner_dir) / "test_info.json"
        cmd = [
            ebm2target,
            "-i",
            ebm_input_file,
            "--debug-unimplemented",
            "--test-info",
            test_info_file.as_posix(),
        ]
        cmd.extend(additional_args)
        print(f"\nRunning command: {' '.join(cmd)}")
        output = sp.check_output(cmd, timeout=60)

        with open(setup_target_file, "wb") as f:
            f.write(output)

elif mode == "test":
    task_dir = unictest_env_vars["UNICTEST_WORK_DIR"]
    test_format_name = unictest_env_vars["UNICTEST_INPUT_FORMAT"]

    if fuzz_mode:
        # In fuzz mode, generate a random binary input via ebm2rmw
        ebm2rmw = (pl.Path(original_workdir) / "tool/ebm2rmw").as_posix()
        if os.name == "nt":
            ebm2rmw += ".exe"
        ebm_input_file = (pl.Path(runner_dir) / "runner_input.ebm").as_posix()
        fuzz_binary_file = (pl.Path(task_dir) / "fuzz_input.bin").as_posix()
        cmd = [
            ebm2rmw,
            "-i", ebm_input_file,
            "--fuzz-generate",
            "--fuzz-seed", fuzz_seed,
            "--fuzz-count", "1",
            "--entry-point", test_format_name,
            "--output-file", fuzz_binary_file,
        ]
        print(f"\nRunning fuzz generator: {' '.join(cmd)}")
        fuzz_gen_result = sp.run(cmd, timeout=60, capture_output=True)
        # Print stderr from ebm2rmw (contains seed info, warnings)
        if fuzz_gen_result.stderr:
            sys.stderr.buffer.write(fuzz_gen_result.stderr)
        if not pl.Path(fuzz_binary_file).exists():
            # Fuzz generator couldn't produce a valid input for this format/seed
            # This is acceptable - exit with decoder failure code
            print(f"Fuzz generator produced no output (format may be too constrained for seed {fuzz_seed})")
            sys.exit(10)
        with open(fuzz_binary_file, "rb") as f:
            input_data = f.read()
        print(f"Fuzz binary generated: {fuzz_binary_file} ({len(input_data)} bytes)")
    else:
        original_input_file = unictest_env_vars["UNICTEST_BINARY_FILE"]
        with open(original_input_file, "rb") as f:
            input_data = f.read()

    input_file = pl.Path(task_dir) / "input.bin"
    with open(input_file, "wb") as f:
        f.write(input_data)
    output_file = pl.Path(task_dir) / "output.bin"
    test_script_file = find_tool_unictest_script(pl.Path(original_workdir), target_command)
    print(f"\nRunning test script: {test_script_file.as_posix()}", flush=True)
    additional_args = unictest_env_vars["UNICTEST_OPTION_SET_RUN_OPTIONS"].split(",")
    optionset_name = unictest_env_vars["UNICTEST_OPTION_SET_NAME"]
    cmds = [
        sys.executable,
        test_script_file.as_posix(),
        setup_target_file.as_posix(),
        input_file.as_posix(),
        output_file.as_posix(),
        test_format_name,
        optionset_name,
    ]
    cmds.extend(additional_args)
    try:
        sp.check_call(
            cmds,
            stdout=sys.stdout,
            stderr=sys.stderr,
        )
    except sp.CalledProcessError as e:
        print(f"Test script failed with return code: {e.returncode}")
        sys.exit(e.returncode)

    if fuzz_mode:
        # In fuzz mode, skip output comparison - the exit code from the
        # test script is what matters (unictest binary handles judgment)
        if output_file.exists():
            print("Fuzz test completed, output file created.")
        else:
            print("Fuzz test completed, no output file (decode may have failed).")
    else:
        if not output_file.exists():
            print(f"Output file not created: {output_file.as_posix()}")
            sys.exit(1)
        with open(output_file, "rb") as f:
            output_data = f.read()
        # do compare and binary diff if not match
        if output_data != input_data:
            print("Output data does not match input data!")
            a = hexdump(input_data)
            b = hexdump(output_data)
            print("Hex dump of input data:")
            print(a)
            print("Hex dump of output data:")
            print(b)
            print("Diff between input and output data:")
            diff = difflib.unified_diff(
                a.splitlines(keepends=True),
                b.splitlines(keepends=True),
                fromfile="input_data",
                tofile="output_data",
            )
            for line in diff:
                print(line, end="")
            sys.exit(1)
        else:
            print("Output data matches input data.")
else:
    print(f"Unknown mode: {mode}")
    sys.exit(1)
