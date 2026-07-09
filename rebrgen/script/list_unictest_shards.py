"""Emit CI shards as a JSON array of ``{"runner", "option_set"}`` objects.

Used by CI to build a dynamic matrix that parallelizes each runner *per option
set* (e.g. ebm2rust's std-io / zero-copy / async / async-zero-copy each become
their own job). This is finer-grained than ``list_unictest_runners.py``, which
emits one shard per runner.

A runner that declares no option sets yields a single shard with an empty
``option_set``; CI omits the ``--target-option-set`` filter in that case so the
runner's default set runs. The runner name and option-set names are read from
each referenced ``unictest_runner.json`` so we stay robust against renames.
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

    shards = []
    for entry in config.get("runners", []):
        runner_file = resolve_file(entry["file"], work_dir)
        with open(runner_file, "r", encoding="utf-8") as f:
            runner = json.load(f)
        name = runner["name"]
        option_sets = runner.get("option_sets") or []
        if option_sets:
            for option_set in option_sets:
                shards.append({"runner": name, "option_set": option_set["name"]})
        else:
            # No declared option sets: one shard, no option-set filter.
            shards.append({"runner": name, "option_set": ""})

    json.dump(shards, sys.stdout)
    return 0


if __name__ == "__main__":
    sys.exit(main())
