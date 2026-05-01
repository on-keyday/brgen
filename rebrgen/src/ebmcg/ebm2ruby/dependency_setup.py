#!/usr/bin/env python3
# Install toolchain/system dependencies for ebm2ruby unictest.
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
import subprocess

HERE = pathlib.Path(__file__).parent


def main() -> None:
    # Use precompiled Ruby binaries (ruby-build) instead of compiling CRuby from source.
    # Compiling from source can take 20+ minutes; the precompiled path is seconds.
    subprocess.check_call(["mise", "settings", "ruby.compile=false"], cwd=HERE)
    subprocess.check_call(["mise", "install"], cwd=HERE)


if __name__ == "__main__":
    main()
