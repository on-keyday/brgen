import sys
import os
import subprocess as sp
import pathlib as pl
import shutil
import platform as plt

if len(sys.argv) != 7:
    exit(1)

INPUT = sys.argv[1]
OUTPUT = sys.argv[2]
ORIGIN = sys.argv[3]
TMPDIR = sys.argv[4]
DEBUG = True if sys.argv[5] == "true" else False
CONFIG = sys.argv[6]

CONFIG_DIR = pl.Path(CONFIG).parent


# run compiler for the test
# add link directory $FUTILS_DIR/lib and -lfutils
# compiler output will be redirected to stdout and stderr
os.mkdir(TMPDIR + "/src")
shutil.copyfile(ORIGIN, TMPDIR + "/src/target.rs")
shutil.copyfile(INPUT, TMPDIR + "/src/main.rs")
shutil.copyfile(
    CONFIG_DIR.joinpath("Cargo.toml").absolute().as_posix(), TMPDIR + "/Cargo.toml"
)

# run cargo build

ret = sp.call(
    ["cargo", "build", "--manifest-path", TMPDIR + "/Cargo.toml"],
    stdout=sys.stdout,
    stderr=sys.stderr,
)

if ret != 0:
    exit(ret)
print("Build successful")
suffix = ".exe" if plt.system() == "Windows" else ""
build_mode = "Debug" if plt.system() == "Windows" else "debug"

print("Copying", TMPDIR + f"/target/{build_mode}/test" + suffix, OUTPUT)
shutil.copyfile(TMPDIR + f"/target/{build_mode}/test" + suffix, OUTPUT)
