"""Emit the runner names declared in ``test/unictest.json`` as a JSON array.

Used by CI to build a dynamic matrix without hard-coding the language list.
The runner name is read from each referenced ``unictest_runner.json`` file so
we stay robust against directory renames.
"""
import json
import pathlib as pl
import sys

DEFAULT_CONFIG = pl.Path("test/unictest.json")


def resolve_file(entry_file: str, work_dir: pl.Path) -> pl.Path:
    return pl.Path(entry_file.replace("$WORK_DIR", str(work_dir))).resolve()


def main() -> int:
    config_path = pl.Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_CONFIG
    work_dir = config_path.resolve().parent.parent

    with open(config_path, "r", encoding="utf-8") as f:
        config = json.load(f)

    names = []
    for entry in config.get("runners", []):
        runner_file = resolve_file(entry["file"], work_dir)
        with open(runner_file, "r", encoding="utf-8") as f:
            runner = json.load(f)
        names.append(runner["name"])

    json.dump(names, sys.stdout)
    return 0


if __name__ == "__main__":
    sys.exit(main())
