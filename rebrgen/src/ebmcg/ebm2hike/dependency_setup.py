#!/usr/bin/env python3
# Install the Hike toolchain for the ebm2hike unictest runner.
#
# Hike (github.com/kanryu/hike-lang) publishes no binary releases, so the
# compiler is built from a checkout with the Go toolchain. hikec is left at the
# checkout root because that is where it resolves `std/` from; unictest.py finds
# it through HIKE_ROOT.
import os
import pathlib
import subprocess

HERE = pathlib.Path(__file__).parent
REPO = "https://github.com/kanryu/hike-lang.git"
CHECKOUT = pathlib.Path(os.environ.get("HIKE_ROOT", HERE / ".hike-lang")).resolve()
HIKEC = "hikec.exe" if os.name == "nt" else "hikec"


def main() -> None:
    if not CHECKOUT.exists():
        subprocess.check_call(["git", "clone", "--depth", "1", REPO, str(CHECKOUT)])
    subprocess.check_call(
        ["go", "build", "-o", HIKEC, "./cmd/hikec"],
        cwd=CHECKOUT,
    )
    built = CHECKOUT / HIKEC
    if not built.exists():
        raise SystemExit(f"hikec was not produced at {built}")
    print(f"hikec built at {built}")
    print(f"set HIKE_ROOT={CHECKOUT} (or HIKEC={built}) to run the ebm2hike unictest")

    github_env = os.environ.get("GITHUB_ENV")
    if github_env:
        with open(github_env, "a", encoding="utf-8") as f:
            f.write(f"HIKE_ROOT={CHECKOUT}\n")


if __name__ == "__main__":
    main()
