#!/usr/bin/env python3
"""Survey which .bgn files ebmgen can convert, and which EBM constructs they carry.

Two questions, over every ``.bgn`` in ``../example/`` and ``src/test/``:

1. **What still fails to convert?** Failures are grouped by ``(message, innermost
   frame)``, not by message alone - ``Unexpected nullptr`` is a single generic
   error raised from one helper, so the message on its own merges unrelated
   causes. The innermost ``at <file>:<line>`` frame is what separates them. This
   grouping is the working list behind ADR 0048.
2. **What would a new test input actually add?** For every convertible ``.bgn``
   outside the unictest corpus, the set of ``Statement|Expression|Type <KIND>``
   tokens in its EBM is compared against the union over the corpus. A candidate
   is only worth wire data if it introduces a kind the corpus lacks.

Kind *presence* is a coarse measure: it ignores how constructs nest, how deep a
recursion goes, and what arithmetic a length expression performs. Two formats
with identical kind sets can still generate very different code. Treat a zero
here as "adds nothing measurable", not as "adds nothing".

Every ebmgen invocation is independent, so they run on a thread pool; each
worker gets its own scratch paths. Use ``-j`` to change the worker count.

Usage::

    python script/survey_bgn_constructs.py              # both sections
    python script/survey_bgn_constructs.py --failures   # conversion failures only
    python script/survey_bgn_constructs.py --constructs # construct diff only
    python script/survey_bgn_constructs.py -j 8         # cap workers
"""

import argparse
import collections
import concurrent.futures
import glob
import json
import os
import re
import subprocess
import sys
import threading

KIND = re.compile(r"\b(Statement|Expression|Type) ([A-Z][A-Z0-9_]+)\b")
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EBMGEN = os.path.join(ROOT, "tool", "ebmgen")
SCRATCH = os.path.join(ROOT, "save", "_survey")

_slots = threading.local()


def slot():
    """Per-thread scratch prefix, so parallel ebmgen runs never share a file."""
    if not hasattr(_slots, "prefix"):
        _slots.prefix = os.path.join(SCRATCH, "w{}".format(threading.get_ident()))
    return _slots.prefix


def corpus_sources():
    """Absolute paths of the .bgn files the unictest corpus already covers."""
    out = set()
    for name in ("test/inputs.json", "test/inputs_utf8.json"):
        path = os.path.join(ROOT, name)
        if not os.path.exists(path):
            continue
        with open(path, encoding="utf-8") as f:
            for entry in json.load(f):
                src = entry["source"].replace("$WORK_DIR/", "")
                out.add(os.path.normpath(os.path.join(ROOT, src)))
    return out


def all_sources():
    """Every candidate .bgn, excluding the archive tree."""
    found = glob.glob(os.path.join(ROOT, "..", "example", "**", "*.bgn"), recursive=True)
    found += glob.glob(os.path.join(ROOT, "src", "test", "*.bgn"))
    keep = []
    for f in found:
        parts = os.path.normpath(f).replace(os.sep, "/").split("/")
        if "archive" not in parts:
            keep.append(os.path.normpath(f))
    return sorted(set(keep))


def convert(bgn):
    """(ebm path, None) on success, or (None, (message, innermost frame))."""
    ebm = slot() + ".ebm"
    r = subprocess.run([EBMGEN, "-i", bgn, "-o", ebm], capture_output=True, text=True)
    out = (r.stderr or r.stdout).strip()
    if r.returncode == 0 and not out:
        return ebm, None
    lines = out.splitlines()
    msg = lines[0].split("Convert Error:")[-1].strip() if lines else "exit {}".format(r.returncode)
    frame = ""
    for ln in lines[1:]:
        if ln.strip().startswith("at "):
            frame = ln.strip()[3:].replace("\\", "/").split("/rebrgen/")[-1]
            break
    return None, (msg[:80], frame)


def kinds(bgn):
    """Set of EBM construct kinds in bgn, or None if it does not convert."""
    ebm, err = convert(bgn)
    if err is not None:
        return None
    txt = slot() + ".txt"
    r = subprocess.run([EBMGEN, "-i", ebm, "-d", txt], capture_output=True, text=True)
    if r.returncode != 0:
        return None
    with open(txt, encoding="utf-8", errors="replace") as f:
        return {"{} {}".format(a, b) for a, b in KIND.findall(f.read())}


def rel(path):
    return os.path.relpath(path, ROOT).replace(os.sep, "/")


def pmap(fn, items, workers):
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as ex:
        return list(ex.map(fn, items))


def report_failures(sources, workers):
    results = pmap(lambda f: (f, convert(f)[1]), sources, workers)
    groups = collections.defaultdict(list)
    for f, err in results:
        if err is not None:
            groups[err].append(f)
    total = sum(len(v) for v in groups.values())
    print("=== .bgn that ebmgen cannot convert: {} in {} groups ===".format(total, len(groups)))
    for key in sorted(groups, key=lambda k: (-len(groups[k]), k)):
        msg, frame = key
        print("  [{}] {}".format(len(groups[key]), msg))
        print("       at {}".format(frame or "(no frame)"))
        for f in sorted(groups[key]):
            print("         {}".format(rel(f)))


def report_constructs(sources, corpus, workers):
    baseline = set()
    for k in pmap(kinds, sorted(corpus), workers):
        if k:
            baseline |= k
    print("=== construct diff ===")
    print("corpus: {} formats, {} distinct constructs".format(len(corpus), len(baseline)))

    cands = [f for f in sources if f not in corpus]
    results = pmap(lambda f: (f, kinds(f)), cands, workers)

    rows = []
    adds = collections.Counter()
    measured = 0
    for f, k in results:
        if not k:
            continue
        measured += 1
        new = k - baseline
        if new:
            rows.append((len(new), rel(f), sorted(new)))
            adds.update(new)
    rows.sort(reverse=True)

    print("candidates measured: {}, of which {} add something".format(measured, len(rows)))
    print()
    for n, f, new in rows:
        print("{:3}  {}".format(n, f))
        print("     {}".format(", ".join(new)))
    print()
    print("=== constructs absent from the corpus, by candidates carrying them ===")
    for kind, count in adds.most_common():
        print("  {:4}  {}".format(count, kind))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--failures", action="store_true", help="only list conversion failures")
    ap.add_argument("--constructs", action="store_true", help="only list the construct diff")
    ap.add_argument("-j", "--jobs", type=int, default=min(16, (os.cpu_count() or 4)),
                    help="parallel ebmgen invocations (default: min(16, cpu count))")
    args = ap.parse_args()

    if not os.path.exists(EBMGEN) and not os.path.exists(EBMGEN + ".exe"):
        print("ebmgen not found at {} - build first".format(EBMGEN), file=sys.stderr)
        return 1

    os.makedirs(SCRATCH, exist_ok=True)
    sources = all_sources()
    corpus = corpus_sources()
    both = not (args.failures or args.constructs)

    if args.failures or both:
        report_failures(sources, args.jobs)
    if both:
        print()
    if args.constructs or both:
        report_constructs(sources, corpus, args.jobs)
    return 0


if __name__ == "__main__":
    sys.exit(main())
