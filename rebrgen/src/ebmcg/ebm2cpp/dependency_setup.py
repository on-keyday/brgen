#!/usr/bin/env python3
# Install toolchain/system dependencies for ebm2cpp unictest.
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
# build_config.template.json sets FUTILS_DIR to "../utils/" (relative to
# rebrgen). unictest.py resolves that to rebrgen/../utils, so futils must
# live there for the generated C++ to compile.
UTILS_TARGET = HERE.parents[2].parent / "utils"


def main() -> None:
    if UTILS_TARGET.exists():
        print(f"ebm2cpp: utils already present at {UTILS_TARGET}")
        return
    print(f"ebm2cpp: cloning futils into {UTILS_TARGET}")
    subprocess.check_call(
        [
            "git",
            "clone",
            "--depth",
            "1",
            "https://github.com/on-keyday/utils.git",
            str(UTILS_TARGET),
        ]
    )


if __name__ == "__main__":
    main()
