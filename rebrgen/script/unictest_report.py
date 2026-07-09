"""Shared failure-reporting helper for unictest runners.

Every ``unictest.py`` and ``unictest_setup.py`` emits, on the failure path, a
single machine-readable line::

    UNICTEST_ERROR: <phase>: <one-line reason>

``render_unictest_summary.py`` greps for this line, so the CI job summary shows
an authoritative per-case reason without guessing over the mixed stdout blob
(which always opens with a ``UNICTEST_*`` env dump and harness scaffolding).

The reason is taken at the *scoped* failure site - the specific subprocess's
captured output, or a value the caller computed - so it never contains that
harness noise. ``phase`` is one of: ``codegen``, ``compile``, ``decode``,
``encode``, ``run``, ``compare``. The caller knows the phase; that classification
can't be recovered downstream from free text.

Import works because ``unictest_setup.py`` puts ``script/`` on the child's
``PYTHONPATH`` before spawning each ``unictest.py`` (see unictest_setup.py). So a
plain ``import unictest_report`` resolves in every runner with no path boilerplate.
"""

import re
import sys

# Lines that read like a real diagnostic, most specific first. These run only
# against *scoped* build output (e.g. one compiler's stderr), where progress
# noise is the only thing to skip - never the harness env dump. For single-line
# reasons (a program's ``Decode failed: X``) selection is bypassed entirely.
_ERRORISH = [
    re.compile(r"error(?:\[[A-Z]?\d+\])?:\s*\S", re.I),         # rust/clang "error:", "error[E0308]:"
    re.compile(r"[\w./\\-]+\.[A-Za-z]\w*:\d+(?::\d+)?:\s*\S"),  # file:line[:col]: msg (go/clang)
    re.compile(r"panic:\s*\S"),                                # go runtime panic
    re.compile(r"error TS\d+:\s*\S"),                          # tsc "error TS2322:"
]


def pick_line(text):
    """Reduce a captured (scoped) blob to its single most error-like line.

    A single-line input is returned verbatim (the common run-phase case). For a
    multi-line compiler/build blob, the first line matching a diagnostic shape
    wins; failing that, the last non-empty line (build tools print the fatal
    conclusion last, and the input is scoped so that line is meaningful).
    """
    lines = [ln.strip() for ln in (text or "").splitlines() if ln.strip()]
    if not lines:
        return ""
    if len(lines) == 1:
        return lines[0][:300]
    for pat in _ERRORISH:
        for ln in lines:
            if pat.search(ln):
                return ln[:300]
    return lines[-1][:300]


def report(phase, text=None):
    """Emit the ``UNICTEST_ERROR:`` line without exiting (caller controls exit).

    Use when the caller still has cleanup or an existing ``sys.exit`` to run
    (e.g. unictest_setup.py prints the hexdump diff, then exits itself).
    """
    line = pick_line(text)
    suffix = ": " + line if line else ""
    print("UNICTEST_ERROR: {}{}".format(phase, suffix), flush=True)


def fail(phase, text=None, code=1):
    """Emit the ``UNICTEST_ERROR:`` line and exit with ``code``."""
    report(phase, text)
    sys.exit(code)
