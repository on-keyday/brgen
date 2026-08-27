#!/usr/bin/env python3
"""nast の解析と src2json の速度を並べて測る。

  python src/core/nast/bench.py [--repeat 3] [--inputs example]

測れること / 測れないことがはっきり分かれるので、先に書いておく。

**同じ土俵に乗るもの**

  parse            .bgn からノードを組むところ
  import 解決      config.import が指すファイルを読んで繋ぐところ
  名前解決         参照を宣言に結びつけるところ
  型付け           式に型を付けるところ (nast はまだ途中)

**乗らないもの**

  - src2json は 1 プロセス 1 ファイルしか受け取らず、成功すると必ず JSON を
    stdout に書く。つまり src2json 側の数字には常に JSON の直列化が入る。
    nast 側にそれに当たる出力は無い (--quiet で何も出さない)。
  - src2json は nast がまだ持たない段も走らせる: available / endian spec /
    explicit error / io operation / metadata / assert / 型属性 / block trait /
    monomorphize。--not-resolve-* で落とせるものは落とすが、全部ではない。
  - 型付けの範囲が違う。nast は到達可能な式の 7 割ほどにしか型を付けていない
    (残りは Call / Cast / Index / If / Match など)。付けていない分だけ速い。

なので出るのは「今の実装どうしの実測」であって、同じ仕事をした場合の比較では
ない。段ごとの内訳 (--time) のほうが、どこに時間が行っているかを見るには使える。

既定ではベンチ用に -O2 で建て直す。build.py の既定は -O0 なので、そのまま
測ると最適化の差が全部乗ってしまう。
"""

import argparse
import os
import subprocess
import sys
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", ".."))
BUILD_DIR = os.path.join(SCRIPT_DIR, "build")
EXE = ".exe" if os.name == "nt" else ""
CORPUS = os.path.join(BUILD_DIR, "nast_corpus" + EXE)
SRC2JSON = os.path.join(REPO_ROOT, "tool", "src2json" + EXE)


def collect(root):
    out = []
    for dirpath, _, names in os.walk(root):
        for n in sorted(names):
            if n.endswith(".bgn"):
                out.append(os.path.join(dirpath, n))
    return sorted(out)


def timed(fn, repeat):
    """一番速かった回を採る。他のプロセスに邪魔された回を混ぜないため。"""
    best = None
    for _ in range(repeat):
        start = time.perf_counter()
        fn()
        elapsed = time.perf_counter() - start
        best = elapsed if best is None else min(best, elapsed)
    return best


def run_src2json(paths, extra):
    for p in paths:
        subprocess.run([SRC2JSON] + extra + [p], stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)


def run_corpus(paths, extra):
    subprocess.run([CORPUS, "--quiet"] + extra + paths, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)


def run_corpus_per_file(paths, extra):
    for p in paths:
        subprocess.run([CORPUS, "--quiet"] + extra + [p], stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)


BENCH_DIR = os.path.join(SCRIPT_DIR, "bench")
BUILD_DIR = os.path.join(SCRIPT_DIR, "build")


def build_tools():
    """bench/ の計測プログラムを建てる。

    build.py と同じ .o を使い回す。何を測るかは bench/README.md にある。
    """
    import glob
    import importlib.util

    spec = importlib.util.spec_from_file_location("nast_build",
                                                  os.path.join(SCRIPT_DIR, "build.py"))
    nb = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(nb)

    compiler = nb.find_compiler(None)
    futils = nb.find_futils(None)
    if not futils:
        sys.exit("error: futils not found; the tools need it")
    cmd = [compiler, "-std=c++23", "-O2",
           "-I", SCRIPT_DIR, "-I", os.path.join(REPO_ROOT, "src"), "-I", futils]
    link = []
    lib = nb.find_futils_lib(futils)
    if lib:
        link.append(lib)
        if os.name == "nt" and os.sep + "Debug" + os.sep in lib:
            cmd += ["-fms-runtime-lib=dll_dbg"]

    shared = [os.path.join(SCRIPT_DIR, n) for n in ("parse.cpp", "stream.cpp", "compare.cpp")]
    shared += sorted(glob.glob(os.path.join(SCRIPT_DIR, "bind", "*.cpp")))
    os.makedirs(BUILD_DIR, exist_ok=True)
    objs = [nb.compile_obj(cmd, src, quiet=True) for src in shared]

    suffix = ".exe" if os.name == "nt" else ""
    for src in sorted(glob.glob(os.path.join(BENCH_DIR, "*.cpp"))):
        name = "nast_" + os.path.splitext(os.path.basename(src))[0]
        out = os.path.join(BUILD_DIR, name + suffix)
        nb.run(cmd + [nb.compile_obj(cmd, src, quiet=True)] + objs + ["-o", out] + link)
        print(f"built: {out}")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--repeat", type=int, default=3)
    p.add_argument("--inputs", default=os.path.join(REPO_ROOT, "example"))
    p.add_argument("--no-build", action="store_true", help="do not rebuild at -O2")
    p.add_argument("--tools", action="store_true",
                   help="build the measurement programs in bench/ and stop")
    args = p.parse_args()

    if args.tools:
        build_tools()
        return

    if not args.no_build:
        subprocess.run([sys.executable, os.path.join(SCRIPT_DIR, "build.py"),
                        "-O", "2", "--no-run", "--no-wire-hpp"], check=True)

    paths = collect(args.inputs)
    if not paths:
        sys.exit(f"no .bgn under {args.inputs}")
    for exe in (CORPUS, SRC2JSON):
        if not os.path.exists(exe):
            sys.exit(f"not built: {exe}")
    # tool/ の中身がどの構成で建てられたかは分からないので、Release の
    # ビルドツリーが無ければ Debug と見て警告する。片方だけ最適化された
    # 数字を並べても意味が無い。
    if not os.path.isdir(os.path.join(REPO_ROOT, "built", "native", "Release")):
        print("WARNING: built/native/Release does not exist, so tool/src2json is")
        print("         probably a Debug build while nast is measured at -O2.")
        print("         Build it with `python build.py native Release` to compare.")
        print("")
    print(f"{len(paths)} files, best of {args.repeat}\n")

    rows = []
    rows.append(("src2json  full + json",
                 timed(lambda: run_src2json(paths, []), args.repeat)))
    rows.append(("src2json  --not-resolve-type + json",
                 timed(lambda: run_src2json(paths, ["--not-resolve-type"]), args.repeat)))
    rows.append(("nast      one process per file",
                 timed(lambda: run_corpus_per_file(paths, []), args.repeat)))
    rows.append(("nast      one process, all files",
                 timed(lambda: run_corpus(paths, []), args.repeat)))
    rows.append(("nast      parse only, one process",
                 timed(lambda: run_corpus(paths, ["--parse-only"]), args.repeat)))

    width = max(len(r[0]) for r in rows)
    for name, sec in rows:
        print(f"{name:<{width}}  {sec * 1000:9.1f} ms  ({sec / len(paths) * 1000:6.2f} ms/file)")

    print("\n段ごとの内訳 (nast, 1 プロセス):")
    subprocess.run([CORPUS, "--quiet", "--time"] + paths)


if __name__ == "__main__":
    main()
