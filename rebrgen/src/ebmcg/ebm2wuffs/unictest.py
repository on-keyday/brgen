#!/usr/bin/env python3
# Test logic for ebm2wuffs.
#
# Current scope: drive the generated .wuffs through `wuffs gen`, the Wuffs
# front-end (parse + type check + bounds/overflow prover + C codegen). Passing
# it means the generated Wuffs is provably memory-safe, not merely syntactically
# valid. The decode/encode round-trip (compile the emitted C and run it against
# INPUT_FILE) is future work — see the TODO at the end.
import os
import pathlib
import shutil
import subprocess
import sys

HERE = pathlib.Path(__file__).parent


def resolve_wuffs_root():
    candidates = []
    env_root = os.environ.get("WUFFS_ROOT")
    if env_root:
        candidates.append(pathlib.Path(env_root))
    candidates.append(HERE / ".wuffs")  # created by dependency_setup.py
    candidates.append(pathlib.Path("C:/workspace/wuffs"))  # local dev fallback
    for c in candidates:
        if (c / "wuffs-root-directory.txt").exists():
            return c.resolve()
    return None


def main():
    TEST_TARGET_FILE = sys.argv[1]  # the generated .wuffs file
    INPUT_FILE = sys.argv[2]
    OUTPUT_FILE = sys.argv[3]
    TEST_TARGET_FORMAT = sys.argv[4]  # struct name (PascalCase)

    print(f"Testing {TEST_TARGET_FILE} (format={TEST_TARGET_FORMAT})")

    wuffs_root = resolve_wuffs_root()
    if wuffs_root is None:
        print("Wuffs toolchain not found. Run this runner's dependency_setup.py "
              "or set WUFFS_ROOT to a wuffs checkout.")
        sys.exit(1)

    env = os.environ.copy()
    gopath = subprocess.check_output(["go", "env", "GOPATH"], text=True).strip()
    env["PATH"] = env.get("PATH", "") + os.pathsep + str(pathlib.Path(gopath) / "bin")
    wuffs_exe = "wuffs.exe" if os.name == "nt" else "wuffs"

    # `wuffs gen` operates on a package directory under <root>/std. Stage the
    # generated file as a throwaway package (unique per process to avoid clashes).
    # Wuffs package names must match [a-z0-9]+ (no underscores).
    pkg = f"ebm2wuffsprobe{os.getpid()}"
    pkgdir = wuffs_root / "std" / pkg
    try:
        if pkgdir.exists():
            shutil.rmtree(pkgdir)
        pkgdir.mkdir(parents=True)
        shutil.copy2(TEST_TARGET_FILE, pkgdir / f"{pkg}.wuffs")

        print(f"\nRunning `wuffs gen` on std/{pkg} ...")
        proc = subprocess.run(
            [wuffs_exe, "gen", "-langs", "c", f"std/{pkg}"],
            cwd=str(wuffs_root),
            env=env,
            capture_output=True,
            text=True,
        )
        if proc.stdout:
            print("--- wuffs gen stdout ---")
            print(proc.stdout)
        if proc.stderr:
            print("--- wuffs gen stderr ---")
            print(proc.stderr)
        if proc.returncode != 0:
            print("wuffs gen failed: generated Wuffs did not pass the type "
                  "checker / bounds prover.")
            sys.exit(10)
        print("wuffs gen succeeded: generated Wuffs compiles to C.")
    finally:
        if pkgdir.exists():
            shutil.rmtree(pkgdir, ignore_errors=True)

    # TODO(ebm2wuffs): decode/encode round-trip. Once formats pass `wuffs gen`,
    # compile the emitted C against the Wuffs base (WUFFS_IMPLEMENTATION) with a
    # small main() that wraps INPUT_FILE in a wuffs_base__io_buffer, calls the
    # generated decode coroutine, re-encodes into another io_buffer, and writes
    # OUTPUT_FILE — then this exits 0/10/20 like the other runners.
    print("round-trip harness not yet implemented for ebm2wuffs (gen-check only).")
    sys.exit(10)


if __name__ == "__main__":
    main()
