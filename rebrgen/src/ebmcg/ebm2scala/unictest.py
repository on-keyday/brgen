#!/usr/bin/env python3
# Test logic for ebm2scala: compile the generated module with scala-cli and
# run a decode -> re-encode round trip against the input file.
import os
import pathlib as pl
import shutil
import subprocess as sp
import sys

import unictest_report


def find_scala_cli() -> str:
    # SCALA_CLI env var wins; otherwise PATH lookup.
    env = os.environ.get("SCALA_CLI")
    if env:
        return env
    found = shutil.which("scala-cli")
    if found:
        return found
    unictest_report.fail("harness", "scala-cli not found (install it or set SCALA_CLI)")
    raise AssertionError("unreachable")


def main():
    TEST_TARGET_FILE = sys.argv[1]  # The generated .scala file
    INPUT_FILE = sys.argv[2]
    OUTPUT_FILE = sys.argv[3]
    TEST_TARGET_FORMAT = sys.argv[4]  # The format (class) name
    OPTION_SET_NAME = sys.argv[5]
    ADDITIONAL_ARGS = sys.argv[6:] if len(sys.argv) > 6 else []

    scala_cli = find_scala_cli()

    work_dir = pl.Path(os.getcwd())
    proj_dir = work_dir / "scala_proj"
    os.makedirs(proj_dir, exist_ok=True)

    module_path = proj_dir / "BgnGenerated.scala"
    with open(TEST_TARGET_FILE, "rb") as src, open(module_path, "wb") as dst:
        dst.write(src.read())

    # Formats are flat classes in the BgnGenerated object; nested layer paths
    # are "_"-joined by the generator.
    format_class = TEST_TARGET_FORMAT.replace(".", "_")

    driver_src = f"""object Main {{
  def main(args: Array[String]): Unit = {{
    val input = java.nio.file.Files.readAllBytes(java.nio.file.Paths.get(args(0)));
    val obj = new BgnGenerated.{format_class}();
    try {{
      obj.decode(new BgnGenerated.BgnInput(input));
    }} catch {{
      case e: Throwable => {{
        System.err.println("Decode error: " + e);
        e.printStackTrace();
        sys.exit(10);
      }}
    }}
    val out = new BgnGenerated.BgnOutput();
    try {{
      obj.encode(out);
    }} catch {{
      case e: Throwable => {{
        System.err.println("Encode error: " + e);
        e.printStackTrace();
        sys.exit(20);
      }}
    }}
    java.nio.file.Files.write(java.nio.file.Paths.get(args(1)), out.toBytes());
    println("Round-trip succeeded, wrote " + out.offset + " bytes");
  }}
}}
"""
    driver_path = proj_dir / "Main.scala"
    with open(driver_path, "w", encoding="utf-8") as f:
        f.write(driver_src)

    print(
        f"Testing ebm2scala ({OPTION_SET_NAME}): format={format_class}, "
        f"module={module_path.name}, driver={driver_path.name}",
        flush=True,
    )

    # --server=false: no Bloop daemon, so parallel unictest batches don't
    # contend on a shared compile server. It is a sub-command option, so it
    # goes after compile/run, not before.
    common = [scala_cli, "--power"]
    result = sp.run(
        common + ["compile", "--server=false", "."],
        cwd=proj_dir,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print("scala-cli compilation failed!")
        print(result.stdout)
        print(result.stderr)
        unictest_report.fail("compile", (result.stdout or "") + (result.stderr or ""))

    print("Compilation successful.")

    cmd = common + ["run", "--server=false", ".", "--main-class", "Main", "--", INPUT_FILE, OUTPUT_FILE]
    print(f"Running: {' '.join(cmd)}", flush=True)

    try:
        # A miscompiled decoder can spin forever; don't wedge the whole batch.
        proc = sp.run(cmd, cwd=proj_dir, capture_output=True, text=True, timeout=120)
    except sp.TimeoutExpired as e:
        stdout = e.stdout
        if isinstance(stdout, bytes):
            stdout = stdout.decode(errors="replace")
        sys.stdout.write(stdout or "")
        unictest_report.fail("run", "driver timed out after 120s (likely infinite decode loop)", code=1)
    sys.stdout.write(proc.stdout or "")
    sys.stderr.write(proc.stderr or "")
    if proc.returncode != 0:
        # The driver exits 10 on decode error, 20 on encode error (stderr
        # carries "Decode error: ..." etc.); anything else is 'run'.
        phase = {10: "decode", 20: "encode"}.get(proc.returncode, "run")
        unictest_report.fail(phase, proc.stderr or proc.stdout, code=proc.returncode)


if __name__ == "__main__":
    main()
