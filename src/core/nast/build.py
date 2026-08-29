#!/usr/bin/env python3
"""nast を建てる。CMake の wrapper。

configure を毎回書かなくて済むようにするためのもので、コンパイルの仕方は
持たない (それは CMakeLists.txt にある)。ビルドの仕方が 2 つあると片方が
必ず腐るので、ここでは CMake を呼ぶだけにする。

  python src/core/nast/build.py                  # 建てて単体テストを走らせる
  python src/core/nast/build.py --no-run         # 建てるだけ
  python src/core/nast/build.py --test           # ctest を全部走らせる
  python src/core/nast/build.py -t nast_corpus   # ターゲットを指定して建てる
  python src/core/nast/build.py --release        # 最適化して建てる (計測用)
  python src/core/nast/build.py --clean          # ツリーを捨ててやり直す
  python src/core/nast/build.py -j 8             # 並列数 (既定は CMake 側の 4)
  python src/core/nast/build.py -D FOO=BAR       # cmake にそのまま渡す

ビルドツリーは構成ごとに分ける:

  build/cmake    Debug   (既定。ctest はこちら)
  build/release  Release (--release。bench.py が使う)

生成 (node/nodes.h / wire/*) はビルドの依存として走るので、ここでは何もしない。
"""

import argparse
import os
import shutil
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_ROOT = os.path.join(SCRIPT_DIR, "build")


def tree_for(release):
    return os.path.join(BUILD_ROOT, "release" if release else "cmake")


def run(cmd):
    print("$", " ".join(cmd), flush=True)
    result = subprocess.run(cmd)
    if result.returncode != 0:
        sys.exit(result.returncode)


def configure(tree, release, defines):
    args = ["cmake", "-S", SCRIPT_DIR, "-B", tree, "-G", "Ninja",
            "-DCMAKE_BUILD_TYPE=" + ("Release" if release else "Debug")]
    for d in defines:
        args.append("-D" + d)
    run(args)


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("-t", "--target", action="append", default=[],
                   help="build only this target (repeatable)")
    p.add_argument("-j", "--jobs", type=int,
                   help="parallel jobs (default: the pool in CMakeLists, 4)")
    p.add_argument("-D", "--define", action="append", default=[],
                   metavar="VAR=VALUE", help="passed to cmake as -DVAR=VALUE")
    p.add_argument("--release", action="store_true",
                   help="build optimized into build/release")
    p.add_argument("--clean", action="store_true",
                   help="delete the build tree first")
    p.add_argument("--reconfigure", action="store_true",
                   help="run cmake configure even if the tree exists")
    p.add_argument("--no-run", action="store_true",
                   help="do not run the unit test after building")
    p.add_argument("--test", action="store_true",
                   help="run ctest (unit test and both round trips)")
    args = p.parse_args()

    tree = tree_for(args.release)
    if args.clean and os.path.isdir(tree):
        print(f"removing {tree}")
        shutil.rmtree(tree)
    if args.reconfigure or not os.path.exists(os.path.join(tree, "CMakeCache.txt")):
        configure(tree, args.release, args.define)
    elif args.define:
        # 既にあるツリーへの -D は configure し直さないと効かない。
        configure(tree, args.release, args.define)

    build = ["cmake", "--build", tree]
    for t in args.target:
        build += ["--target", t]
    if args.jobs:
        build += ["-j", str(args.jobs)]
    run(build)

    if args.test:
        run(["ctest", "--test-dir", tree, "--output-on-failure"])
        return
    if not args.no_run and not args.target:
        exe = os.path.join(tree, "bin", "nast_test" + (".exe" if os.name == "nt" else ""))
        if os.path.exists(exe):
            run([exe])


if __name__ == "__main__":
    main()
