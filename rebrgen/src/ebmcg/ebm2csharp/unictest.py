#!/usr/bin/env python3
# Test logic for ebm2csharp: compile the generated module with the dotnet SDK
# and run a decode -> re-encode round trip against the input file.
import json
import os
import pathlib as pl
import subprocess as sp
import sys

import unictest_report

CSPROJ = """<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>Exe</OutputType>
    <TargetFramework>net8.0</TargetFramework>
    <RollForward>LatestMajor</RollForward>
    <Nullable>disable</Nullable>
    <ImplicitUsings>disable</ImplicitUsings>
    <AssemblyName>csharp_proj</AssemblyName>
    <StartupObject>Program</StartupObject>
  </PropertyGroup>
</Project>
"""


def main():
    TEST_TARGET_FILE = sys.argv[1]  # The generated .cs file
    INPUT_FILE = sys.argv[2]
    OUTPUT_FILE = sys.argv[3]
    TEST_TARGET_FORMAT = sys.argv[4]  # The format (nested class) name
    OPTION_SET_NAME = sys.argv[5]
    ADDITIONAL_ARGS = sys.argv[6:] if len(sys.argv) > 6 else []

    work_dir = pl.Path(os.getcwd())
    proj_dir = work_dir / "csharp_proj"
    os.makedirs(proj_dir, exist_ok=True)

    module_path = proj_dir / "BgnGenerated.cs"
    with open(TEST_TARGET_FILE, "rb") as src, open(module_path, "wb") as dst:
        dst.write(src.read())

    driver_src = f"""public class Program {{
    public static int Main(string[] args) {{
        byte[] input = System.IO.File.ReadAllBytes(args[0]);
        BgnGenerated.{TEST_TARGET_FORMAT} obj = new BgnGenerated.{TEST_TARGET_FORMAT}();
        // Round-trip through the System.IO.Stream overloads so both the
        // stream boundary and the underlying BgnInput/BgnOutput codec are
        // exercised.
        try {{
            obj.decode(new System.IO.MemoryStream(input));
        }} catch (System.Exception e) {{
            System.Console.Error.WriteLine("Decode error: " + e);
            return 10;
        }}
        System.IO.MemoryStream outMs = new System.IO.MemoryStream();
        try {{
            obj.encode(outMs);
        }} catch (System.Exception e) {{
            System.Console.Error.WriteLine("Encode error: " + e);
            return 20;
        }}
        System.IO.File.WriteAllBytes(args[1], outMs.ToArray());
        System.Console.WriteLine("Round-trip succeeded, wrote " + outMs.Length + " bytes");
        return 0;
    }}
}}
"""
    with open(proj_dir / "Program.cs", "w", encoding="utf-8") as f:
        f.write(driver_src)
    with open(proj_dir / "csharp_proj.csproj", "w", encoding="utf-8") as f:
        f.write(CSPROJ)

    print(
        f"Testing ebm2csharp ({OPTION_SET_NAME}): format={TEST_TARGET_FORMAT}, "
        f"module={module_path.name}",
        flush=True,
    )

    env = dict(os.environ)
    env.setdefault("DOTNET_CLI_TELEMETRY_OPTOUT", "1")
    env.setdefault("DOTNET_NOLOGO", "1")
    # dotnet emits localized messages in the console codepage on Windows while
    # compiler snippets stay UTF-8; force English + decode as UTF-8 so the
    # capture never dies with UnicodeDecodeError (cp932).
    env.setdefault("DOTNET_CLI_UI_LANGUAGE", "en")

    result = sp.run(
        ["dotnet", "build", "-nologo", "-v", "quiet"],
        cwd=proj_dir,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        env=env,
    )
    if result.returncode != 0:
        print("dotnet build failed!")
        print(result.stdout)
        print(result.stderr)
        unictest_report.fail("compile", (result.stdout or "") + (result.stderr or ""))

    print("Compilation successful.")

    dll_path = proj_dir / "bin" / "Debug" / "net8.0" / "csharp_proj.dll"
    cmd = ["dotnet", str(dll_path), INPUT_FILE, OUTPUT_FILE]
    print(f"Running: {' '.join(cmd)}", flush=True)
    print("for VSCode debugging")
    print(
        json.dumps(
            {
                "type": "coreclr",
                "request": "launch",
                "cwd": os.getcwd(),
                "name": f"Debug ebm2csharp unictest ({TEST_TARGET_FORMAT})",
                "program": "dotnet",
                "args": [str(dll_path), INPUT_FILE, OUTPUT_FILE],
            },
            indent=4,
        )
    )

    try:
        # A miscompiled decoder can spin forever; don't wedge the whole batch.
        proc = sp.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60, env=env)
    except sp.TimeoutExpired as e:
        sys.stdout.write(e.stdout or "")
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
