#!/usr/bin/env python3
# Install toolchain/system dependencies for ebm2rmw unictest.
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

HERE = pathlib.Path(__file__).parent


def main() -> None:
    print("dependency setup not implemented for ebm2rmw")


if __name__ == "__main__":
    main()
