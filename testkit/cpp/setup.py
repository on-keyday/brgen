import sys
import os
import subprocess as sp
import pathlib as pl
import shutil
import platform as plt

INPUT = sys.argv[1]
OUTPUT = sys.argv[2]
ORIGIN = sys.argv[3]
TMPDIR = sys.argv[4]
DEBUG = True if sys.argv[5] == "true" else False

if DEBUG:
    with open(ORIGIN, "r", encoding="utf-8") as fp:
        print(ORIGIN)
        print(fp.read())

with open(INPUT, "r", encoding="utf-8") as f:
    ABS_ORIGIN = pl.Path(ORIGIN).absolute().as_posix()
    TEXT = f.read()
    if DEBUG:
        print(INPUT, " before replace")
    REPLACED_INPUT = TEXT.replace(ORIGIN, ABS_ORIGIN)

with open(INPUT, "w", encoding="utf-8") as f:
    f.write(REPLACED_INPUT)
    if DEBUG:
        print(INPUT, " after replace")
        print(REPLACED_INPUT)

FUTILS_DIR = os.environ.get("FUTILS_DIR")
if FUTILS_DIR is None:
    print("FUTILS_DIR not set", file=sys.stderr)
    exit(2)


# run compiler for the test
# add link directory $FUTILS_DIR/lib and -lfutils
# compiler output will be redirected to stdout and stderr
CC = "clang++"
CMDLINE = [
    CC,
    "-std=c++20",
    "-I",
    FUTILS_DIR + "/src/include",
    "-L",
    FUTILS_DIR + "/lib",
    "-lfutils",
    INPUT,
    "-o",
    OUTPUT,
    "-g",
]

# use dynamic linking crt on windows
if plt.system() == "Windows":
    CMDLINE.append("-nostdlib")
    CMDLINE.append("-fms-runtime-lib=dll_dbg")
else:
    CMDLINE.append("-stdlib=libc++")

print(f"Compiling {INPUT} to {OUTPUT} with {CMDLINE} ")
code = sp.call(
    CMDLINE,
    stdout=sys.stdout,
    stderr=sys.stderr,
)
if code != 0:
    exit(code)

suffix = ".so"

match plt.system():
    case "Linux":
        suffix = ".so"
    case "Darwin":
        suffix = ".dylib"
    case "Windows":
        suffix = ".dll"

shutil.copyfile(FUTILS_DIR + "/lib/libfutils" + suffix, TMPDIR + "/libfutils" + suffix)
