#!/usr/bin/env python3
"""Diff the current unictest run against the previous recorded run.

Emits a GitHub-flavoured Markdown report (for ``$GITHUB_STEP_SUMMARY``) of:

- **Regressions** - cases that were passing in the previous recorded run and
  fail now. The actionable bit; shown with the current first error line.
- **Fixes** - cases that were failing and pass now.
- **Flaky candidates** - cases whose pass/fail flips repeatedly over the last
  few recorded runs (computed from ``summary.jsonl``).
- Per-runner totals with the change in failure count.

This is **record-only / non-gating** (see ADR 0036): it never fails the job, so
flaky tests don't turn CI red. The previous run is the most recent snapshot
already in the history store (this runs *before* the current run is recorded).
"""

import argparse
import glob
import json
import os
import sys

# Reuse the trimming/identity helpers so case keys match the recorder exactly.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from record_unictest_history import _case_key, _first_error_line  # noqa: E402


def _load_current(path):
    """case_key -> {'success': bool, 'error': str|None, fields...} for this run."""
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    out = {}
    for r in data.get("results", []):
        out[_case_key(r)] = {
            "success": bool(r.get("success")),
            "error": _first_error_line(r),
            "runner": r.get("runner", "?"),
            "input_name": r.get("input_name", "?"),
            "option_set": r.get("option_set", "?"),
        }
    return out


def _latest_snapshot(history_dir):
    """Return (sha, {case_key: success}) of the most recent recorded run, or None."""
    summary_path = os.path.join(history_dir, "summary.jsonl")
    if not os.path.exists(summary_path):
        return None
    last = None
    with open(summary_path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line:
                last = json.loads(line)
    if not last or not last.get("sha"):
        return None
    snap_path = os.path.join(history_dir, "runs", "{}.json".format(last["sha"]))
    if not os.path.exists(snap_path):
        return None
    with open(snap_path, encoding="utf-8") as f:
        snap = json.load(f)
    statuses = {_case_key(r): bool(r.get("success")) for r in snap.get("results", [])}
    return last["sha"], statuses


def _flaky_candidates(history_dir, window):
    """Cases whose status flips >=2 times across the last `window` recorded runs."""
    summary_path = os.path.join(history_dir, "summary.jsonl")
    if not os.path.exists(summary_path):
        return []
    runs = []
    with open(summary_path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line:
                runs.append(json.loads(line))
    runs = runs[-window:]
    if len(runs) < 3:
        return []
    # status sequence per case: fail if in that run's `fails`, else pass.
    seqs = {}
    for run in runs:
        failset = set(run.get("fails", []))
        for key in failset:
            seqs.setdefault(key, [])
    # We only have failure lists, so reconstruct per-case sequences over the
    # union of cases that ever failed in the window.
    tracked = set()
    for run in runs:
        tracked |= set(run.get("fails", []))
    flaky = []
    for key in tracked:
        seq = ["fail" if key in set(run.get("fails", [])) else "pass" for run in runs]
        flips = sum(1 for a, b in zip(seq, seq[1:]) if a != b)
        if flips >= 2:
            flaky.append((key, flips))
    flaky.sort(key=lambda kv: (-kv[1], kv[0]))
    return flaky


def _totals(statuses_or_current, is_current):
    """runner -> [pass, fail]. Accepts current-map or snapshot-status-map."""
    totals = {}
    if is_current:
        for info in statuses_or_current.values():
            b = totals.setdefault(info["runner"], [0, 0])
            b[0 if info["success"] else 1] += 1
    else:
        for key, ok in statuses_or_current.items():
            runner = key.split("/", 1)[0]
            b = totals.setdefault(runner, [0, 0])
            b[0 if ok else 1] += 1
    return totals


def build_report(current, prev, flaky):
    cur_status = {k: v["success"] for k, v in current.items()}
    lines = []
    if prev is None:
        lines.append("## unictest history — first recorded run")
        lines.append("")
        lines.append("No previous snapshot to diff against; baseline established.")
        lines.append("")
    else:
        prev_sha, prev_status = prev
        regressions = sorted(
            k for k, ok in cur_status.items()
            if not ok and prev_status.get(k) is True
        )
        fixes = sorted(
            k for k, ok in cur_status.items()
            if ok and prev_status.get(k) is False
        )
        new_cases = sorted(set(cur_status) - set(prev_status))
        gone_cases = sorted(set(prev_status) - set(cur_status))
        # A newly-added runner or input is not a regression (it never passed
        # before), but a new case that *fails* is still worth itemizing.
        new_failing = [k for k in new_cases if cur_status.get(k) is False]

        lines.append("## unictest diff vs `{}`".format(prev_sha[:12]))
        lines.append("")
        lines.append(
            "🔴 **{} regression(s)** · 🟢 {} fix(es) · ⚠️ {} flaky · "
            "🆕 {} new ({} failing) · ➖ {} removed".format(
                len(regressions), len(fixes), len(flaky),
                len(new_cases), len(new_failing), len(gone_cases)
            )
        )
        lines.append("")

        if regressions:
            lines.append("### 🔴 Regressions (was passing, now failing)")
            lines.append("")
            lines.append("| runner | input | option | error |")
            lines.append("|---|---|---|---|")
            for k in regressions:
                info = current[k]
                err = (info.get("error") or "").replace("|", "\\|")[:160]
                lines.append("| {} | {} | {} | {} |".format(
                    info["runner"], info["input_name"], info["option_set"], err))
            lines.append("")

        if fixes:
            lines.append("### 🟢 Fixes (was failing, now passing)")
            lines.append("")
            for k in fixes:
                lines.append("- `{}`".format(k))
            lines.append("")

        if new_failing:
            lines.append("### 🆕 New cases that fail (newly added runner/input)")
            lines.append("")
            lines.append("| runner | input | option | error |")
            lines.append("|---|---|---|---|")
            for k in new_failing:
                info = current[k]
                err = (info.get("error") or "").replace("|", "\\|")[:160]
                lines.append("| {} | {} | {} | {} |".format(
                    info["runner"], info["input_name"], info["option_set"], err))
            lines.append("")

    if flaky:
        lines.append("### ⚠️ Flaky candidates (status flips in recent runs)")
        lines.append("")
        lines.append("| case | flips |")
        lines.append("|---|---|")
        for key, flips in flaky:
            lines.append("| `{}` | {} |".format(key, flips))
        lines.append("")

    # Per-runner totals (with Δfail vs previous if available).
    cur_tot = _totals(current, True)
    prev_tot = _totals(prev[1], False) if prev else {}
    lines.append("### Totals per runner")
    lines.append("")
    lines.append("| runner | pass | fail | Δfail |")
    lines.append("|---|---|---|---|")
    for runner in sorted(cur_tot):
        p, fcount = cur_tot[runner]
        if not prev:
            delta = ""
        elif runner not in prev_tot:
            delta = "new"  # runner didn't exist last run; not a regression
        else:
            d = fcount - prev_tot[runner][1]
            delta = "±0" if d == 0 else ("+{}".format(d) if d > 0 else str(d))
        lines.append("| {} | {} | {} | {} |".format(runner, p, fcount, delta))
    lines.append("")
    return "\n".join(lines), (len(regressions) if prev else 0)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--current", required=True, help="this run's test_results.json")
    ap.add_argument("--history-dir", required=True, help="history store (worktree)")
    ap.add_argument("--out", default="-", help="markdown output path ('-' = stdout)")
    ap.add_argument("--flaky-window", type=int, default=10)
    args = ap.parse_args()

    try:
        sys.stdout.reconfigure(encoding="utf-8")  # robust on Windows cp932 consoles
    except Exception:
        pass

    current = _load_current(args.current)
    prev = _latest_snapshot(args.history_dir)
    flaky = _flaky_candidates(args.history_dir, args.flaky_window)
    report, regression_count = build_report(current, prev, flaky)

    if args.out == "-":
        sys.stdout.write(report + "\n")
    else:
        with open(args.out, "a", encoding="utf-8") as f:
            f.write(report + "\n")

    # Non-gating: report only. Surface the count on stderr for the CI log.
    print("regressions: {}".format(regression_count), file=sys.stderr)


if __name__ == "__main__":
    main()
