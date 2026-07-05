#!/usr/bin/env python3
"""Record a unictest run into the append-only history store.

Given a merged ``test_results.json`` (as produced by
``merge_unictest_results.py``), this writes a trimmed per-run snapshot to
``<out-dir>/runs/<sha>.json`` and regenerates ``<out-dir>/summary.jsonl`` from
every snapshot found under ``runs/``.

The history store lives on the orphan ``unictest-history`` branch (see ADR /
docs). ``runs/<sha>.json`` is unique per commit, so concurrent CI runs never
conflict on it; ``summary.jsonl`` is *derived* (regenerated from all runs),
which keeps it conflict-free under rebase-retry as well.

To bound growth without losing triage ability, snapshots keep the full
``output_stdout`` / ``output_stderr`` (each capped) *only for failing cases* -
those are what you investigate later. Passing cases keep just the boolean and
identifying fields ("compilation successful" logs have no triage value).

Setup failures (``setup_failures`` in the merged results) are recorded too:
they never appear in ``results``, so without them a runner whose codegen
errors out looks like its cases silently "didn't run" (e.g. ebm2llvm shows 1
case instead of 51). They are keyed as ``runner/<source basename>/option`` -
a distinct namespace from result cases (the ``.bgn`` suffix marks them).
"""

import argparse
import glob
import json
import os
import re

# Identifying fields kept for every case (pass or fail).
_KEEP_FIELDS = (
    "runner",
    "input_name",
    "option_set",
    "format_name",
    "success",
    "failure_case",
    "status_interpretation",
)

# Full logs are kept only for failures; cap each blob to guard against a
# pathological case (e.g. a huge runtime byte dump) bloating the snapshot.
_LOG_CAP = 32 * 1024

_ERROR_LINE = re.compile(r"^\s*(error(?:\[E\d+\])?:.*)$", re.MULTILINE)


def _cap(text):
    """Truncate an over-long log blob, leaving a marker."""
    if text is None:
        return None
    if len(text) <= _LOG_CAP:
        return text
    return text[:_LOG_CAP] + "\n...[truncated {} bytes]".format(len(text) - _LOG_CAP)


def _first_error_line(entry):
    """Best-effort first compiler/runner error line, for failure triage."""
    if entry.get("success"):
        return None
    for key in ("error_message", "output_stderr", "output_stdout"):
        text = entry.get(key)
        if not text:
            continue
        m = _ERROR_LINE.search(text)
        if m:
            return m.group(1).strip()[:300]
    return None


def _case_key(entry):
    return "{}/{}/{}".format(
        entry.get("runner", "?"),
        entry.get("input_name", "?"),
        entry.get("option_set", "?"),
    )


def _setup_key(entry):
    """Key for a trimmed setup-failure entry (source keeps its .bgn suffix, so
    this can never collide with a result case key)."""
    return "{}/{}/{}".format(
        entry.get("runner", "?"),
        entry.get("source", "?"),
        entry.get("option_set", "?"),
    )


def _trim_setup(raw):
    """Normalize one raw setup_failures entry (serde enum-wrapped) for storage.

    ``CommandFailure`` carries runner/source/option + logs; ``OrchestrationError``
    only an error message. Unknown variants are kept with their name so new
    unictest failure kinds surface instead of vanishing.
    """
    if not isinstance(raw, dict) or len(raw) != 1:
        return {"kind": "Unknown", "error_message": _cap(json.dumps(raw, ensure_ascii=False))}
    kind, body = next(iter(raw.items()))
    out = {"kind": kind}
    if not isinstance(body, dict):
        out["error_message"] = _cap(str(body))
        return out
    if "runner" in body:
        out["runner"] = body["runner"]
    if "source" in body:
        # paths are machine-dependent; the basename is the stable identity
        out["source"] = os.path.basename(body["source"])
    if "option_set" in body:
        out["option_set"] = body["option_set"]
    err = _first_error_line(body)
    if err is None:
        # setup errors are often "<tool>: error: ..." which the ^error: regex
        # misses; fall back to the first non-empty stderr line
        for line in (body.get("output_stderr") or "").splitlines():
            if line.strip():
                err = line.strip()[:300]
                break
    if err is not None:
        out["error"] = err
    for key in ("output_stdout", "output_stderr", "error_message"):
        val = _cap(body.get(key))
        if val:
            out[key] = val
    return out


def _trim(entry):
    out = {k: entry[k] for k in _KEEP_FIELDS if k in entry}
    if entry.get("success"):
        return out
    # Failure: keep full (capped) logs for later triage.
    err = _first_error_line(entry)
    if err is not None:
        out["error"] = err
    for key in ("output_stdout", "output_stderr", "error_message"):
        val = _cap(entry.get(key))
        if val:
            out[key] = val
    if entry.get("output_status") is not None:
        out["output_status"] = entry["output_status"]
    return out


def write_snapshot(results_path, sha, date, out_dir):
    with open(results_path, encoding="utf-8") as f:
        data = json.load(f)
    results = data.get("results", [])
    snapshot = {
        "sha": sha,
        "date": date,
        "results": [_trim(r) for r in results],
        "setup_failures": [_trim_setup(s) for s in data.get("setup_failures", [])],
    }
    runs_dir = os.path.join(out_dir, "runs")
    os.makedirs(runs_dir, exist_ok=True)
    snap_path = os.path.join(runs_dir, "{}.json".format(sha))
    with open(snap_path, "w", encoding="utf-8") as f:
        json.dump(snapshot, f, ensure_ascii=False, separators=(",", ":"), sort_keys=True)
        f.write("\n")
    return snap_path


def _summarize(snapshot):
    """Aggregate one snapshot into a compact summary line."""
    totals = {}
    fails = []
    setup_fails = []
    orchestration_errors = 0
    for r in snapshot.get("results", []):
        runner = r.get("runner", "?")
        bucket = totals.setdefault(runner, {"pass": 0, "fail": 0})
        if r.get("success"):
            bucket["pass"] += 1
        else:
            bucket["fail"] += 1
            fails.append(_case_key(r))
    for s in snapshot.get("setup_failures", []):
        if "runner" not in s:
            # OrchestrationError etc. - not attributable to a runner/source
            orchestration_errors += 1
            continue
        bucket = totals.setdefault(s["runner"], {"pass": 0, "fail": 0})
        bucket["setup_fail"] = bucket.get("setup_fail", 0) + 1
        setup_fails.append(_setup_key(s))
    line = {
        "sha": snapshot.get("sha"),
        "date": snapshot.get("date"),
        "totals": totals,
        "fails": sorted(fails),
        "setup_fails": sorted(setup_fails),
    }
    if orchestration_errors:
        line["orchestration_errors"] = orchestration_errors
    return line


def regenerate_summary(out_dir):
    """Rebuild summary.jsonl deterministically from every runs/*.json."""
    lines = []
    for path in glob.glob(os.path.join(out_dir, "runs", "*.json")):
        with open(path, encoding="utf-8") as f:
            snapshot = json.load(f)
        lines.append(_summarize(snapshot))
    # Sort by (date, sha) so the file is a stable, ordered timeline.
    lines.sort(key=lambda s: (s.get("date") or "", s.get("sha") or ""))
    summary_path = os.path.join(out_dir, "summary.jsonl")
    with open(summary_path, "w", encoding="utf-8") as f:
        for line in lines:
            json.dump(line, f, ensure_ascii=False, separators=(",", ":"), sort_keys=True)
            f.write("\n")
    return summary_path, len(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--results", required=True, help="merged test_results.json")
    ap.add_argument("--sha", required=True, help="commit SHA that was tested")
    ap.add_argument("--date", required=True, help="run timestamp (ISO 8601)")
    ap.add_argument("--out-dir", required=True, help="history store root (worktree)")
    args = ap.parse_args()

    snap = write_snapshot(args.results, args.sha, args.date, args.out_dir)
    summary, count = regenerate_summary(args.out_dir)
    print("recorded {} ; summary now spans {} run(s) -> {}".format(snap, count, summary))


if __name__ == "__main__":
    main()
