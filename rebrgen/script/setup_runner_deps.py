#!/usr/bin/env python3
"""Invoke a runner's dependency_setup.py before unictest runs.

Called by CI with the runner name (e.g. ``ebm2rust``). Resolves the runner
dir (``src/ebmcg/<name>`` or ``src/ebmip/<name>``) and executes the
``dependency_setup.py`` inside it with the current Python interpreter.
"""
import pathlib as pl
import subprocess as sp
import sys


def resolve_runner_dir(name: str) -> pl.Path:
    for sub in ("ebmcg", "ebmip"):
        d = pl.Path("src") / sub / name
        if d.is_dir():
            return d
    raise FileNotFoundError(
        f"runner dir not found for {name} in src/ebmcg or src/ebmip"
    )


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <runner_name>", file=sys.stderr)
        return 2
    runner = sys.argv[1]
    runner_dir = resolve_runner_dir(runner)
    script = runner_dir / "dependency_setup.py"
    if not script.exists():
        print(
            f"error: {script} not found. "
            f"Every runner must ship a dependency_setup.py "
            f"(scaffolded by script/ebmcodegen.py).",
            file=sys.stderr,
        )
        return 1
    print(f"running {script}", flush=True)
    return sp.call([sys.executable, str(script)])


if __name__ == "__main__":
    sys.exit(main())
