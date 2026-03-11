import subprocess as sp
import sys

LANG_LIST = ["c", "python", "haskell", "go", "cpp3", "kaitai"]
INPUT = "src/test/simple_case.bgn" if len(sys.argv) < 2 else sys.argv[1]
BUILD_MODE = "native" if len(sys.argv) < 3 else sys.argv[2]
BUILD_TYPE = "Debug" if len(sys.argv) < 4 else sys.argv[3]
SRC2JSON = (
    "C:/workspace/shbrgen/brgen/tool/src2json" if len(sys.argv) < 5 else sys.argv[4]
)

for lang in LANG_LIST:
    sp.run(
        ["python", "script/bm/gen_template.py", lang, BUILD_MODE, BUILD_TYPE],
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )

sp.run(
    ["python", "script/bm/collect_cmake.py", INPUT, SRC2JSON],
    check=True,
    stdout=sys.stdout,
    stderr=sys.stderr,
)

sp.run(
    ["python", "script/bm/generate_test_glue.py"],
    check=True,
    stdout=sys.stdout,
    stderr=sys.stderr,
)

DOC = sp.check_output(["tool/gen_template", "--mode", "docs-markdown"])

with open("docs/template_parameters.md", "wb") as f:
    f.write(DOC)
