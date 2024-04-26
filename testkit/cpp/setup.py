import sys

INPUT = sys.argv[1]
OUTPUT = sys.argv[2]

import os

FUTILS_DIR = os.environ.get("FUTILS_DIR")
if FUTILS_DIR is None:
    print("FUTILS_DIR not set", file=sys.stderr)
    exit(2)

import subprocess as sp

# run compiler for the test
# add link directory $FUTILS_DIR/lib and -lfutils
# compiler output will be redirected to stdout and stderr
CC = "clang++"

sp.call(
    [
        CC,
        "-std=c++20",
        "-I",
        FUTILS_DIR + "/include",
        "-L",
        FUTILS_DIR + "/lib",
        "-lfutils",
        INPUT,
        "-o",
        OUTPUT,
    ],
    stdout=sys.stdout,
    stderr=sys.stderr,
)
