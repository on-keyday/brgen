import json
import os
import subprocess as sp
import sys
import pathlib as pl

try:
    with open("build_config.json", "r") as f:
        build_config = json.load(f)
    print("Loaded build_config.json", flush=True)
except FileNotFoundError:
    print("build_config.json not found")
    exit(1)

BRGEN_DIR = build_config.get("BRGEN_DIR", "./brgen/")
TOOL_PATH = pl.Path(BRGEN_DIR) / "tool"
UNICTEST = TOOL_PATH / "unictest"
if os.name == "nt":
    UNICTEST = UNICTEST.with_suffix(".exe")

TEST_CASE_FILE = "test/unictest.json"

try:
    sp.check_call(
        [
            UNICTEST.as_posix(),
            "-c",
            TEST_CASE_FILE,
            "--save-tmp-dir",
            "--clean-tmp",
            "--shorter-path",
        ]
        + sys.argv[1:],
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
except sp.CalledProcessError as e:
    sys.exit(e.returncode)
