#!/usr/bin/env python3
"""parse.cpp の旧 AST 前提の記述を nast 向けに機械変換する補助。

clang の診断を読んで、以下だけを繰り返し直す:

  x->f      x が Node<T> のとき  ->  x.ref(a)->f
  x->loc    x が Node<T> のとき  ->  x.ref(a).loc()
  x->loc    x が Ref<A,T> のとき ->  x.loc()

判断はコンパイラの位置情報だけに任せ、こちらでは構文解析しない。
1 回のパスで直せるのは 1 行 1 箇所ではないので、収束するまで回す。

  python src/core/nast/fixup.py [--file parse.cpp] [--max-round 40]
"""

import argparse
import os
import re
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", ".."))
FUTILS_INCLUDE = "C:/workspace/utils_backup/src/include"

# member reference type 'brgen::nast::Node<...>' is not a pointer
NOT_PTR = re.compile(
    r"^(?P<file>.+?):(?P<line>\d+):(?P<col>\d+): error: member reference type "
    r"'(?:brgen::nast::)?Node<.*?>'.*? is not a pointer"
)
# no member named 'loc' in 'brgen::nast::NodeData<...>'  (Ref 経由で loc を触った)
NO_LOC = re.compile(
    r"^(?P<file>.+?):(?P<line>\d+):(?P<col>\d+): error: no member named 'loc' in "
    r"'(?:brgen::nast::)?NodeData<"
)


def compile_diags(path, futils):
    cmd = [
        "clang++", "-std=c++23", "-fsyntax-only", "-ferror-limit=0",
        "-I", SCRIPT_DIR, "-I", futils, "-I", os.path.join(REPO_ROOT, "src"),
        path,
    ]
    return subprocess.run(cmd, capture_output=True, text=True).stderr.splitlines()


def collect_edits(diags):
    """(line, col, kind) を集める。kind: 'ref' か 'loc'"""
    edits = []
    for d in diags:
        m = NOT_PTR.match(d)
        if m:
            edits.append((int(m["line"]), int(m["col"]), "ref"))
            continue
        m = NO_LOC.match(d)
        if m:
            edits.append((int(m["line"]), int(m["col"]), "loc"))
    return edits


def apply_edits(path, edits):
    """右から左へ適用する (同一行に複数あると列がずれるため)"""
    with open(path, encoding="utf-8") as f:
        lines = f.read().split("\n")
    applied = 0
    for line, col, kind in sorted(set(edits), key=lambda e: (-e[0], -e[1])):
        i = line - 1
        if i >= len(lines):
            continue
        text = lines[i]
        pos = col - 1
        if kind == "ref":
            # col は '->' の位置。手前に .ref(a) を差し込む。
            if text[pos:pos + 2] != "->":
                continue
            lines[i] = text[:pos] + ".ref(a)" + text[pos:]
        else:
            # col は 'loc' の位置。直前の '->' ごと .loc() に置き換える。
            if text[pos:pos + 3] != "loc":
                continue
            arrow = text.rfind("->", 0, pos)
            if arrow < 0 or arrow + 2 != pos:
                continue
            lines[i] = text[:arrow] + ".loc()" + text[pos + 3:]
        applied += 1
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    return applied


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--file", default="parse.cpp")
    ap.add_argument("--futils", default=FUTILS_INCLUDE)
    ap.add_argument("--max-round", type=int, default=40)
    args = ap.parse_args()

    path = os.path.join(SCRIPT_DIR, args.file)
    for rnd in range(1, args.max_round + 1):
        diags = compile_diags(path, args.futils)
        errors = sum(1 for d in diags if ": error:" in d)
        edits = collect_edits(diags)
        if not edits:
            print(f"round {rnd}: errors={errors}, no more mechanical fixes")
            return 0 if errors == 0 else 1
        applied = apply_edits(path, edits)
        print(f"round {rnd}: errors={errors}, applied={applied}")
        if applied == 0:
            print("no progress; stopping")
            return 1
    print("hit --max-round")
    return 1


if __name__ == "__main__":
    sys.exit(main())
