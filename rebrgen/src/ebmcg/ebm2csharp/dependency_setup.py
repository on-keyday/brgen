#!/usr/bin/env python3
# Install toolchain/system dependencies for ebm2csharp unictest.
# Invoked by CI before running unictest for this runner.
#
# Pick one (or combine) as needed:
#   - mise:   write mise.toml next to this file, then
#             subprocess.check_call(["mise", "install"], cwd=HERE)
#   - apt:    subprocess.check_call(["sudo", "apt-get", "install", "-y", "<pkg>"])
#   - curl:   download a release tarball and extract
#
# Replace the placeholder body when real dependencies appear.
import pathlib
import shutil
import subprocess

HERE = pathlib.Path(__file__).parent


def main() -> None:
    # GitHub-hosted runners (and most dev machines) already carry a .NET SDK;
    # only report what is available so a missing SDK fails loudly here rather
    # than as a confusing per-test compile error.
    dotnet = shutil.which("dotnet")
    if dotnet is None:
        raise SystemExit(
            "dotnet SDK not found: install .NET SDK 8+ (e.g. apt-get install dotnet-sdk-8.0)"
        )
    version = subprocess.run(
        [dotnet, "--version"], capture_output=True, text=True, check=True
    ).stdout.strip()
    print(f"dotnet SDK found: {version} at {dotnet}")


if __name__ == "__main__":
    main()
