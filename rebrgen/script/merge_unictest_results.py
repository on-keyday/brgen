"""Merge per-runner unictest result JSONs produced by the sharded CI pipeline.

Each shard writes a ``test_results.json`` with the schema declared in
``src/tool/unictest/src/main.rs::TestResults``. We concatenate ``results`` and
``setup_failures`` across shards and recompute the aggregate counts so the
downstream consumer (``unictest_result.html`` etc.) sees a single merged file
indistinguishable from a non-sharded run.
"""
import json
import pathlib as pl
import sys


def merge(shard_paths: list[pl.Path]) -> dict:
    results = []
    setup_failures = []
    for path in shard_paths:
        with open(path, "r", encoding="utf-8") as f:
            shard = json.load(f)
        results.extend(shard.get("results", []))
        setup_failures.extend(shard.get("setup_failures", []))

    fail_count = sum(1 for r in results if not r.get("success"))
    return {
        "success_count": len(results) - fail_count,
        "fail_count": fail_count,
        "setup_fail_count": len(setup_failures),
        "setup_failures": setup_failures,
        "results": results,
    }


def main() -> int:
    if len(sys.argv) < 3:
        print(
            "usage: merge_unictest_results.py <shard_dir> <output_json>",
            file=sys.stderr,
        )
        return 2

    shard_dir = pl.Path(sys.argv[1])
    output_path = pl.Path(sys.argv[2])

    shard_paths = sorted(shard_dir.rglob("test_results.json"))
    if not shard_paths:
        print(f"no shard results found under {shard_dir}", file=sys.stderr)
        return 1

    merged = merge(shard_paths)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(merged, f, indent=2)

    print(
        f"merged {len(shard_paths)} shards -> {output_path} "
        f"(pass={merged['success_count']}, fail={merged['fail_count']}, "
        f"setup_fail={merged['setup_fail_count']})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
