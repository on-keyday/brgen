#!/usr/bin/env python3
# Test logic for ebm2java: compile the generated module with javac and run a
# decode -> re-encode round trip against the input file.
import json
import os
import pathlib as pl
import subprocess as sp
import sys

import unictest_report


def main():
    TEST_TARGET_FILE = sys.argv[1]  # The generated .java file
    INPUT_FILE = sys.argv[2]
    OUTPUT_FILE = sys.argv[3]
    TEST_TARGET_FORMAT = sys.argv[4]  # The format (nested class) name
    OPTION_SET_NAME = sys.argv[5]
    ADDITIONAL_ARGS = sys.argv[6:] if len(sys.argv) > 6 else []

    work_dir = pl.Path(os.getcwd())
    proj_dir = work_dir / "java_proj"
    os.makedirs(proj_dir, exist_ok=True)

    # The generator emits `public class BgnGenerated` (--class-name default);
    # javac requires the file name to match the public class.
    module_path = proj_dir / "BgnGenerated.java"
    with open(TEST_TARGET_FILE, "rb") as src, open(module_path, "wb") as dst:
        dst.write(src.read())

    driver_src = f"""public class Main {{
    public static void main(String[] args) throws Exception {{
        byte[] input = java.nio.file.Files.readAllBytes(java.nio.file.Paths.get(args[0]));
        BgnGenerated.{TEST_TARGET_FORMAT} obj = new BgnGenerated.{TEST_TARGET_FORMAT}();
        // Round-trip through the java.io stream overloads so both the stream
        // boundary and the underlying BgnInput/BgnOutput codec are exercised.
        try {{
            obj.decode(new java.io.ByteArrayInputStream(input));
        }} catch (Throwable e) {{
            System.err.println("Decode error: " + e);
            e.printStackTrace();
            System.exit(10);
        }}
        java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
        try {{
            obj.encode(out);
        }} catch (Throwable e) {{
            System.err.println("Encode error: " + e);
            e.printStackTrace();
            System.exit(20);
        }}
        java.nio.file.Files.write(java.nio.file.Paths.get(args[1]), out.toByteArray());
        System.out.println("Round-trip succeeded, wrote " + out.size() + " bytes");
    }}
}}
"""
    driver_path = proj_dir / "Main.java"
    with open(driver_path, "w", encoding="utf-8") as f:
        f.write(driver_src)

    print(
        f"Testing ebm2java ({OPTION_SET_NAME}): format={TEST_TARGET_FORMAT}, "
        f"module={module_path.name}, driver={driver_path.name}",
        flush=True,
    )

    result = sp.run(
        ["javac", "BgnGenerated.java", "Main.java"],
        cwd=proj_dir,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print("javac compilation failed!")
        print(result.stdout)
        print(result.stderr)
        unictest_report.fail("compile", (result.stdout or "") + (result.stderr or ""))

    print("Compilation successful.")

    cmd = ["java", "-cp", str(proj_dir), "Main", INPUT_FILE, OUTPUT_FILE]
    print(f"Running: {' '.join(cmd)}", flush=True)
    print("for VSCode debugging")
    print(
        json.dumps(
            {
                "type": "java",
                "request": "launch",
                "cwd": os.getcwd(),
                "name": f"Debug ebm2java unictest ({TEST_TARGET_FORMAT})",
                "mainClass": "Main",
                "classPaths": [str(proj_dir)],
                "args": [INPUT_FILE, OUTPUT_FILE],
            },
            indent=4,
        )
    )

    try:
        # A miscompiled decoder can spin forever; don't wedge the whole batch.
        proc = sp.run(cmd, capture_output=True, text=True, timeout=60)
    except sp.TimeoutExpired as e:
        sys.stdout.write((e.stdout or b"").decode(errors="replace") if isinstance(e.stdout, bytes) else (e.stdout or ""))
        unictest_report.fail("run", "driver timed out after 60s (likely infinite decode loop)", code=1)
    sys.stdout.write(proc.stdout or "")
    sys.stderr.write(proc.stderr or "")
    if proc.returncode != 0:
        # The driver exits 10 on decode error, 20 on encode error (stderr
        # carries "Decode error: ..." etc.); anything else is 'run'.
        phase = {10: "decode", 20: "encode"}.get(proc.returncode, "run")
        unictest_report.fail(phase, proc.stderr or proc.stdout, code=proc.returncode)


if __name__ == "__main__":
    main()
