import os
import subprocess as sp
import sys

sp.run(
    ["python","script/build.py"],
    check=True,
    stdout=sys.stdout,
    stderr=sys.stderr,
)
INPUT = "src/test/simple_case.bgn" if len(sys.argv) < 2 else sys.argv[1]
print(f"Input: {INPUT}")
save = sp.check_output(
    ["C:/workspace/shbrgen/brgen/tool/src2json", INPUT],
    stderr=sys.stderr,
)
with open("save/sample.json", "wb") as f:
    f.write(save)
print("Generated: save/sample.json")
save = sp.check_output(
    ["tool/bmgen", "-p", "-i", "save/sample.json", "-o", "save/save.bin", "-c", "save/save.dot","--print-process-time"],
    stderr=sys.stderr,
)
print("Generated: save/save.bin")
print("Generated: save/save.dot")
with open("save/save.txt", "wb") as f:
    f.write(save)
print("Generated: save/save.txt")
save = sp.check_output(
    ["tool/bmgen", "-p", "-i", "save/sample.json", "--print-only-op","--print-process-time"],
    stderr=sys.stderr,
)
with open("save/save_op.txt", "wb") as f:
    f.write(save)
print("Generated: save/save_op.txt")

os.makedirs("save/c", exist_ok=True)

src = sp.check_output(
    ["tool/bm2c", "-i", "save/save.bin","--test-info","save/c/save.c.json"],
    stderr=sys.stderr,
)
cached = False
if os.path.exists("save/c/save.c"):
    with open("save/c/save.c", "rb") as f:
        if f.read() == src:
            print(f"Cached: save/c/save.c")
            cached = True
if not cached:
    with open("save/c/save.c", "wb") as f:
        f.write(src)
    print(f"Generated: save/c/save.c")
os.makedirs("save/cpp", exist_ok=True)

src = sp.check_output(
    ["tool/bm2cpp", "-i", "save/save.bin","--test-info","save/cpp/save.cpp.json"],
    stderr=sys.stderr,
)
cached = False
if os.path.exists("save/cpp/save.cpp"):
    with open("save/cpp/save.cpp", "rb") as f:
        if f.read() == src:
            print(f"Cached: save/cpp/save.cpp")
            cached = True
if not cached:
    with open("save/cpp/save.cpp", "wb") as f:
        f.write(src)
    print(f"Generated: save/cpp/save.cpp")
os.makedirs("save/cpp3", exist_ok=True)

src = sp.check_output(
    ["tool/bm2cpp3", "-i", "save/save.bin","--test-info","save/cpp3/save.cpp.json"],
    stderr=sys.stderr,
)
cached = False
if os.path.exists("save/cpp3/save.cpp"):
    with open("save/cpp3/save.cpp", "rb") as f:
        if f.read() == src:
            print(f"Cached: save/cpp3/save.cpp")
            cached = True
if not cached:
    with open("save/cpp3/save.cpp", "wb") as f:
        f.write(src)
    print(f"Generated: save/cpp3/save.cpp")
os.makedirs("save/go", exist_ok=True)

src = sp.check_output(
    ["tool/bm2go", "-i", "save/save.bin","--test-info","save/go/save.go.json"],
    stderr=sys.stderr,
)
cached = False
if os.path.exists("save/go/save.go"):
    with open("save/go/save.go", "rb") as f:
        if f.read() == src:
            print(f"Cached: save/go/save.go")
            cached = True
if not cached:
    with open("save/go/save.go", "wb") as f:
        f.write(src)
    print(f"Generated: save/go/save.go")
os.makedirs("save/haskell", exist_ok=True)

src = sp.check_output(
    ["tool/bm2haskell", "-i", "save/save.bin","--test-info","save/haskell/save.hs.json"],
    stderr=sys.stderr,
)
cached = False
if os.path.exists("save/haskell/save.hs"):
    with open("save/haskell/save.hs", "rb") as f:
        if f.read() == src:
            print(f"Cached: save/haskell/save.hs")
            cached = True
if not cached:
    with open("save/haskell/save.hs", "wb") as f:
        f.write(src)
    print(f"Generated: save/haskell/save.hs")
os.makedirs("save/kaitai", exist_ok=True)

src = sp.check_output(
    ["tool/bm2kaitai", "-i", "save/save.bin","--test-info","save/kaitai/save.ksy.json"],
    stderr=sys.stderr,
)
cached = False
if os.path.exists("save/kaitai/save.ksy"):
    with open("save/kaitai/save.ksy", "rb") as f:
        if f.read() == src:
            print(f"Cached: save/kaitai/save.ksy")
            cached = True
if not cached:
    with open("save/kaitai/save.ksy", "wb") as f:
        f.write(src)
    print(f"Generated: save/kaitai/save.ksy")
os.makedirs("save/python", exist_ok=True)

src = sp.check_output(
    ["tool/bm2python", "-i", "save/save.bin","--test-info","save/python/save.py.json"],
    stderr=sys.stderr,
)
cached = False
if os.path.exists("save/python/save.py"):
    with open("save/python/save.py", "rb") as f:
        if f.read() == src:
            print(f"Cached: save/python/save.py")
            cached = True
if not cached:
    with open("save/python/save.py", "wb") as f:
        f.write(src)
    print(f"Generated: save/python/save.py")
os.makedirs("save/rust", exist_ok=True)

src = sp.check_output(
    ["tool/bm2rust", "-i", "save/save.bin","--test-info","save/rust/save.rs.json"],
    stderr=sys.stderr,
)
cached = False
if os.path.exists("save/rust/save.rs"):
    with open("save/rust/save.rs", "rb") as f:
        if f.read() == src:
            print(f"Cached: save/rust/save.rs")
            cached = True
if not cached:
    with open("save/rust/save.rs", "wb") as f:
        f.write(src)
    print(f"Generated: save/rust/save.rs")
