#!/usr/bin/env python3
# Install toolchain/system dependencies for ebm2zig unictest.
# Invoked by CI before running unictest for this runner.
import pathlib
import subprocess
import tomllib

HERE = pathlib.Path(__file__).parent


def main() -> None:
    subprocess.check_call(["mise", "install"], cwd=HERE)
    # `mise install` alone leaves the zig shim resolving via the local
    # mise.toml — fine if every consumer cwds into this directory, but
    # unictest.py invokes `zig` from the generated zig_proj directory
    # where no mise.toml is visible, producing
    # "mise ERROR No version is set for shim: zig".
    # Mirror the local pins into the global config so the shim resolves
    # regardless of cwd. Pins are read from mise.toml so they stay in
    # sync.
    with open(HERE / "mise.toml", "rb") as f:
        cfg = tomllib.load(f)
    for tool, version in cfg.get("tools", {}).items():
        subprocess.check_call(["mise", "use", "-g", f"{tool}@{version}"])


if __name__ == "__main__":
    main()
