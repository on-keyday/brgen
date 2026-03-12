import sys
import subprocess as sp

CMPTEST_PATH = (
    "C:/workspace/shbrgen/brgen/tool/cmptest" if len(sys.argv) < 2 else sys.argv[1]
)

exit(
    sp.call(
        [
            CMPTEST_PATH,
            "-f",
            "./testkit/test_info.json",
            "-c",
            "./testkit/cmptest.json",
            "--clean-tmp",
            "--save-tmp-dir",
            "--debug",
        ],
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
)
