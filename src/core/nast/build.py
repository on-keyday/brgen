#!/usr/bin/env python3
"""nast を単体でビルドする。

brgen 本体の CMake ビルド (build.py native / rebrgen script/build.py) には依存しない。
nodes.json -> nodegen.py -> nodes.h を再生成し、test.cpp をコンパイルして実行する。

  python src/core/nast/build.py                # 生成 + ビルド + 実行
  python src/core/nast/build.py --syntax-only  # 生成 + 構文チェックのみ
  python src/core/nast/build.py --no-generate  # 既存の nodes.h でビルド
  python src/core/nast/build.py --compiler g++ --std c++23

nodes.h 自体は futils に依存しないが、test.cpp は as_json の検証に
futils::json::Stringer<> を使う。futils が見つからない場合はその項目だけ落として
残りをビルドする (--no-futils で明示的に落とすこともできる)。
futils の探索順:
  1. --futils <dir>
  2. 環境変数 FUTILS_DIR      (brgen 本体の CMakeLists と同じ)
  3. <repo>/utils             (script/clone_utils.sh の clone 先)
  4. rebrgen/build_config.json の FUTILS_DIR
"""

import argparse
import json
import os
import shutil
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", ".."))
BUILD_DIR = os.path.join(SCRIPT_DIR, "build")
NODEGEN = os.path.join(SCRIPT_DIR, "nodegen.py")
NODES_H = os.path.join(SCRIPT_DIR, "nodes.h")
TEST_CPP = os.path.join(SCRIPT_DIR, "test.cpp")

# 本体と同じく C++23 / clang を既定にする。無ければ順に探す。
DEFAULT_COMPILERS = ["clang++", "g++", "c++"]

# futils のヘッダはこのサブディレクトリにある
FUTILS_INCLUDE_SUBDIR = os.path.join("src", "include")
# 存在確認に使うヘッダ (test.cpp が使うもの)
FUTILS_PROBE = os.path.join("json", "stringer.h")


def find_compiler(explicit):
    if explicit:
        found = shutil.which(explicit)
        if not found:
            sys.exit(f"error: compiler not found: {explicit}")
        return found
    for name in DEFAULT_COMPILERS:
        found = shutil.which(name)
        if found:
            return found
    sys.exit("error: no C++ compiler found (tried: " + ", ".join(DEFAULT_COMPILERS) + ")")


def as_include_dir(futils_dir):
    """futils のルートを受け取り、有効ならインクルードディレクトリを返す"""
    if not futils_dir:
        return None
    include = os.path.join(futils_dir, FUTILS_INCLUDE_SUBDIR)
    if os.path.exists(os.path.join(include, FUTILS_PROBE)):
        return include
    return None


def find_futils(explicit):
    if explicit:
        include = as_include_dir(explicit)
        if not include:
            sys.exit(f"error: futils not found under {explicit} (expected {FUTILS_INCLUDE_SUBDIR}/{FUTILS_PROBE})")
        return include

    include = as_include_dir(os.environ.get("FUTILS_DIR"))
    if include:
        return include

    include = as_include_dir(os.path.join(REPO_ROOT, "utils"))
    if include:
        return include

    config = os.path.join(REPO_ROOT, "rebrgen", "build_config.json")
    if os.path.exists(config):
        try:
            with open(config, encoding="utf-8") as f:
                include = as_include_dir(json.load(f).get("FUTILS_DIR"))
            if include:
                return include
        except (OSError, ValueError):
            pass

    return None


def run(cmd):
    print("$", " ".join(cmd), flush=True)
    result = subprocess.run(cmd)
    if result.returncode != 0:
        sys.exit(result.returncode)


def main():
    parser = argparse.ArgumentParser(description="build nast standalone")
    parser.add_argument("--compiler", help="C++ compiler to use (default: clang++ if available)")
    parser.add_argument("--std", default="c++23", help="language standard (default: c++23)")
    parser.add_argument("--futils", help="futils root directory (overrides autodetection)")
    parser.add_argument("--no-futils", action="store_true", help="skip the as_json check even if futils is available")
    parser.add_argument("--no-generate", action="store_true", help="skip regenerating nodes.h")
    parser.add_argument("--syntax-only", action="store_true", help="compile check only, do not link or run")
    parser.add_argument("--no-run", action="store_true", help="build but do not run the test")
    parser.add_argument("-O", "--optimize", default="0", help="optimization level passed as -O<level> (default: 0)")
    args = parser.parse_args()

    if not args.no_generate:
        run([sys.executable, NODEGEN])
    if not os.path.exists(NODES_H):
        sys.exit(f"error: {NODES_H} not found (run without --no-generate)")

    compiler = find_compiler(args.compiler)
    cmd = [compiler, f"-std={args.std}", f"-O{args.optimize}", "-I", SCRIPT_DIR]

    if args.no_futils:
        print("note: skipping the as_json check (--no-futils)")
    else:
        futils_include = find_futils(args.futils)
        if futils_include:
            cmd += ["-I", futils_include, "-DNAST_TEST_WITH_JSON"]
        else:
            print("note: futils not found; skipping the as_json check "
                  "(set FUTILS_DIR or pass --futils to enable it)")

    if args.syntax_only:
        run(cmd + ["-fsyntax-only", TEST_CPP])
        print("syntax OK")
        return

    os.makedirs(BUILD_DIR, exist_ok=True)
    exe = os.path.join(BUILD_DIR, "nast_test" + (".exe" if os.name == "nt" else ""))
    run(cmd + [TEST_CPP, "-o", exe])
    print(f"built: {exe}")

    if not args.no_run:
        run([exe])


if __name__ == "__main__":
    main()
