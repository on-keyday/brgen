#!/usr/bin/env python3
"""Render a single unictest run into a GitHub Actions job summary.

Unlike ``diff_unictest_history.py`` (which needs the history store and only runs
on ``main`` pushes), this is **history-free**: it turns one merged
``test_results.json`` into GitHub-flavoured Markdown so *every* e2e run - PRs and
``workflow_dispatch`` included - shows an inline pass/fail table on its run page.
The interactive ``unictest_result.html`` viewer stays the artifact for deep
dives; this is the glanceable, ephemeral view.

Job Summary renders Markdown + a sanitized HTML subset (``<details>``/``<table>``
survive; ``<script>``/``<style>``/class CSS are stripped), and caps each step at
1 MiB. So failures go in collapsed ``<details>`` blocks with the first error
line only, and long lists are truncated with an explicit dropped-count note
rather than silently.
"""

import argparse
import os
import re
import sys

# Reuse the recorder's identity/trimming helpers so keys and error extraction
# match the history pipeline exactly.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from record_unictest_history import (  # noqa: E402
    _case_key,
    _setup_key,
    _trim_setup,
)
import json  # noqa: E402

# Cap itemized rows so a fully-red run can't blow past the 1 MiB summary limit.
# Dropped rows are reported, never silently swallowed.
_MAX_ROWS = 400


def _cell(text):
    """Escape a value for a Markdown table cell."""
    return (text or "").replace("|", "\\|").replace("\n", " ")[:200]


# The authoritative reason: runners that use unictest_report emit exactly this
# line at their scoped failure site (see script/unictest_report.py). Preferred
# over any heuristic - it carries the phase and a clean, noise-free reason.
_UNICTEST_ERROR = re.compile(r"^UNICTEST_ERROR:\s*(.+)$", re.M)

# Transitional fallback for runners not yet migrated to unictest_report. unictest
# wraps every run: stdout always opens with a ``UNICTEST_*`` env dump and harness
# scaffolding (``Running test script:``, ``Testing X with Y``, ``Test script
# failed with return code: N``), so "first non-empty line" is always noise.
# Instead we look *positively* for lines that read like an error. Ordered by
# specificity: an actual compiler diagnostic beats a generic marker, so a Go type
# error wins over the "compilation failed!" banner above it. Delete this once
# every runner emits UNICTEST_ERROR:.
_SIGNAL_PATTERNS = [
    re.compile(r"error(?:\[[A-Z]?\d+\])?:\s*\S.*", re.I),        # rust/clang "error:", "error[E0308]:"
    re.compile(r"[\w./\\-]+\.[A-Za-z]\w*:\d+(?::\d+)?:\s*\S.*"),  # file:line[:col]: msg (go/clang)
    re.compile(r"panic:\s*\S.*"),                                # go runtime panic
    re.compile(r".*\bnot implemented yet\b.*", re.I),            # unimplemented test harness
    re.compile(r".*\bcompilation failed\b.*", re.I),            # go/rust compile banner
    re.compile(r".*can'?t open file.*", re.I),                   # missing script / path error
]


def _error_summary(entry):
    """Extract a one-line failure reason for a failing case, or None.

    Prefers the authoritative ``UNICTEST_ERROR:`` line emitted by the runner;
    falls back to the heuristic ``_SIGNAL_PATTERNS`` for un-migrated runners.
    Env dumps and harness scaffolding never match either path, so None (rendered
    as an em dash) is honest — the ``status`` column still classifies it and the
    artifact keeps the full logs.
    """
    blob = "\n".join(
        entry.get(k) or ""
        for k in ("output_stderr", "output_stdout", "error_message")
    )
    m = _UNICTEST_ERROR.search(blob)
    if m:
        return m.group(1).strip()[:300]
    lines = [ln.strip() for ln in blob.splitlines() if ln.strip()]
    for pat in _SIGNAL_PATTERNS:
        for ln in lines:
            if pat.search(ln):
                return ln[:300]
    return None


def _load(path):
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    results = data.get("results", [])
    setups = [_trim_setup(s) for s in data.get("setup_failures", [])]
    return results, setups


def _runner_totals(results, setups):
    """runner -> {'pass', 'fail', 'setup'}."""
    totals = {}
    for r in results:
        b = totals.setdefault(r.get("runner", "?"), {"pass": 0, "fail": 0, "setup": 0})
        b["pass" if r.get("success") else "fail"] += 1
    for s in setups:
        runner = s.get("runner")
        if runner is None:
            continue  # OrchestrationError etc. - not attributable to a runner
        b = totals.setdefault(runner, {"pass": 0, "fail": 0, "setup": 0})
        b["setup"] += 1
    return totals


def _emit_table(lines, header, rows):
    """Append a Markdown table, truncating to _MAX_ROWS with an explicit note."""
    lines.append("| " + " | ".join(header) + " |")
    lines.append("|" + "|".join("---" for _ in header) + "|")
    shown = rows[:_MAX_ROWS]
    for row in shown:
        lines.append("| " + " | ".join(row) + " |")
    if len(rows) > _MAX_ROWS:
        lines.append(
            "| _…{} more row(s) omitted (see the `unictest-results` "
            "artifact)_ |{}".format(len(rows) - _MAX_ROWS, "|" * (len(header) - 1))
        )
    lines.append("")


def build_report(results, setups):
    fails = [r for r in results if not r.get("success")]
    passes = len(results) - len(fails)
    # Setup failures with no runner (orchestration errors) are counted but can't
    # be attributed to a runner in the per-runner table.
    orchestration = [s for s in setups if s.get("runner") is None]
    attributed_setups = [s for s in setups if s.get("runner") is not None]

    lines = []
    lines.append("## unictest results")
    lines.append("")
    icon = "🟢" if not fails and not setups else "🔴"
    lines.append(
        "{} **{} passed** · **{} failed** · **{} setup failure(s)**{}".format(
            icon, passes, len(fails), len(setups),
            " · **{} orchestration error(s)**".format(len(orchestration))
            if orchestration else "",
        )
    )
    lines.append("")

    # Per-runner totals - the glanceable core, always shown.
    totals = _runner_totals(results, attributed_setups)
    lines.append("### Totals per runner")
    lines.append("")
    rows = []
    for runner in sorted(totals):
        b = totals[runner]
        mark = "✅" if b["fail"] == 0 and b["setup"] == 0 else "❌"
        rows.append([
            mark, runner, str(b["pass"]), str(b["fail"]),
            str(b["setup"]) if b["setup"] else "",
        ])
    _emit_table(lines, ["", "runner", "pass", "fail", "setup fail"], rows)

    # Failing cases - collapsed so a green run stays compact.
    if fails:
        lines.append("<details>")
        lines.append("<summary>🔴 {} failing case(s)</summary>".format(len(fails)))
        lines.append("")
        rows = []
        for r in sorted(fails, key=_case_key):
            rows.append([
                _cell(r.get("runner", "?")),
                _cell(r.get("input_name", "?")),
                _cell(r.get("option_set", "?")),
                _cell(r.get("status_interpretation")),
                _cell(_error_summary(r) or "—"),
            ])
        _emit_table(lines, ["runner", "input", "option", "status", "error"], rows)
        lines.append("</details>")
        lines.append("")

    # Setup failures - codegen/setup errored, so the cases never ran.
    if setups:
        lines.append("<details>")
        lines.append(
            "<summary>🟠 {} setup failure(s) (codegen/setup errored; cases did "
            "not run)</summary>".format(len(setups))
        )
        lines.append("")
        rows = []
        for s in sorted(setups, key=_setup_key):
            rows.append([
                _cell(s.get("runner", "(orchestration)")),
                _cell(s.get("source", "?")),
                _cell(s.get("option_set", "?")),
                _cell(s.get("error") or s.get("error_message") or "(no error line captured)"),
            ])
        _emit_table(lines, ["runner", "source", "option", "error"], rows)
        lines.append("</details>")
        lines.append("")

    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--results", required=True, help="merged test_results.json")
    ap.add_argument("--out", default="-", help="markdown output path ('-' = stdout)")
    args = ap.parse_args()

    try:
        sys.stdout.reconfigure(encoding="utf-8")  # robust on Windows cp932 consoles
    except Exception:
        pass

    results, setups = _load(args.results)
    report = build_report(results, setups)

    if args.out == "-":
        sys.stdout.write(report + "\n")
    else:
        with open(args.out, "a", encoding="utf-8") as f:
            f.write(report + "\n")


if __name__ == "__main__":
    main()
