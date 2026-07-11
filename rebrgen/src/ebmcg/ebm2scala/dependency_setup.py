#!/usr/bin/env python3
# Install toolchain/system dependencies for ebm2scala unictest.
# Invoked by CI before running unictest for this runner.
#
# scala-cli is pinned via mise.toml (aqua backend). Same shim caveat as
# ebm2zig: unictest.py invokes `scala-cli` from a generated scala_proj
# directory where no mise.toml is visible, so the local pins are mirrored
# into the global mise config.
import pathlib
import subprocess
import tomllib

HERE = pathlib.Path(__file__).parent


def main() -> None:
    subprocess.check_call(["mise", "install"], cwd=HERE)
    with open(HERE / "mise.toml", "rb") as f:
        cfg = tomllib.load(f)
    for tool, version in cfg.get("tools", {}).items():
        subprocess.check_call(["mise", "use", "-g", f"{tool}@{version}"])


if __name__ == "__main__":
    main()
