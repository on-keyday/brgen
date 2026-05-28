#!/usr/bin/env python3
# Install toolchain/system dependencies for ebm2wuffs unictest.
# Invoked by CI before running unictest for this runner.
#
# Wuffs (https://github.com/google/wuffs) is a Go project that transpiles .wuffs
# to C. We need three CLI tools on PATH: wuffsfmt (formatter / syntax gate),
# wuffs (orchestrator), and wuffs-c (the C backend that `wuffs gen` shells out
# to). `go install ...@latest` fails because the module zip contains a non-ASCII
# filename, so we clone the repo and `go install` from local source instead.
import os
import pathlib
import subprocess

HERE = pathlib.Path(__file__).parent
WUFFS_REPO = "https://github.com/google/wuffs.git"
# WUFFS_ROOT: where the wuffs repo lives. `wuffs gen` locates it by walking up
# for wuffs-root-directory.txt, so unictest.py runs the tool from this dir.
WUFFS_ROOT = pathlib.Path(os.environ.get("WUFFS_ROOT", HERE / ".wuffs")).resolve()


def main() -> None:
    if not (WUFFS_ROOT / "wuffs-root-directory.txt").exists():
        WUFFS_ROOT.parent.mkdir(parents=True, exist_ok=True)
        print(f"Cloning Wuffs into {WUFFS_ROOT} ...")
        subprocess.check_call(
            ["git", "clone", "--depth", "1", WUFFS_REPO, str(WUFFS_ROOT)]
        )
    else:
        print(f"Wuffs repo already present at {WUFFS_ROOT}")

    for cmd in ("wuffsfmt", "wuffs", "wuffs-c"):
        print(f"go install ./cmd/{cmd} ...")
        subprocess.check_call(["go", "install", f"./cmd/{cmd}"], cwd=str(WUFFS_ROOT))

    gopath = subprocess.check_output(["go", "env", "GOPATH"], text=True).strip()
    print(f"Wuffs tools installed to {gopath}{os.sep}bin (ensure it is on PATH)")
    print(f"WUFFS_ROOT={WUFFS_ROOT}")


if __name__ == "__main__":
    main()
