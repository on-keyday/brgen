import sys
import shutil
import subprocess as sp
import re

INPUT = sys.argv[1]
OUTPUT = sys.argv[2]
ORIGIN = sys.argv[3]
TMPDIR = sys.argv[4]

# replace `package xxx\n` (xxx is any string) with `package main\n`
COMPILED = re.compile("package (.*)\n")

with open(INPUT, "r") as fp:
    TEXT = fp.read()
    TEXT = COMPILED.sub("main", TEXT)


with open(INPUT, "w") as fp:
    fp.write(TEXT)

# copy test target
shutil.copyfile(ORIGIN, TMPDIR + "/target.go")
CMDLINE = ["go", "build", "-o", OUTPUT, "."]
print(f"Compiling {INPUT} to {OUTPUT} with {CMDLINE} ")
code = sp.call(
    CMDLINE,
    stdout=sys.stdout,
    stderr=sys.stderr,
    cwd=TMPDIR,
)
if code != 0:
    exit(code)
