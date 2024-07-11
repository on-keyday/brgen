import sys
import os
import subprocess as sp
import pathlib as pl
import shutil
import platform as plt

if len(sys.argv) != 7:
    exit(0)

INPUT = sys.argv[1]
OUTPUT = sys.argv[2]
ORIGIN = sys.argv[3]
TMPDIR = sys.argv[4]
DEBUG = True if sys.argv[5] == "true" else False
CONFIG = sys.argv[6]

if DEBUG:
    with open(ORIGIN, "r", encoding="utf-8") as fp:
        print(ORIGIN)
        print(fp.read())

with open(INPUT, "r", encoding="utf-8") as f:
    ABS_ORIGIN = pl.Path(ORIGIN).absolute().as_posix()
    TEXT = f.read()
    if DEBUG:
        print(INPUT, " before replace")
        print(TEXT)
    REPLACED_INPUT = TEXT.replace(ORIGIN, ABS_ORIGIN)

with open(INPUT, "w", encoding="utf-8") as f:
    f.write(REPLACED_INPUT)
    if DEBUG:
        print(INPUT, " after replace")
        print(REPLACED_INPUT)


# run compiler for the test
# add link directory $FUTILS_DIR/lib and -lfutils
# compiler output will be redirected to stdout and stderr
shutil.copyfile(ORIGIN, TMPDIR + "/target.rs")
shutil.copyfile()
