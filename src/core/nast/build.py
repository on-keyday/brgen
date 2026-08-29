#!/usr/bin/env python3
"""nast を単体でビルドする。

brgen 本体の CMake ビルド (build.py native / rebrgen script/build.py) には依存しない。
nodes.json -> nodegen.py -> nodes.h を再生成し、test.cpp (単体テスト) と
corpus.cpp (.bgn を食わせる driver) をコンパイルして、test のほうを実行する。
あわせて clangd 用の compile_commands.json を書く。

  python src/core/nast/build.py                # 生成 + ビルド + 実行
  python src/core/nast/build.py --syntax-only  # 生成 + 構文チェックのみ
  python src/core/nast/build.py --no-generate  # 既存の nodes.h でビルド
  python src/core/nast/build.py --no-corpus    # test.cpp だけ建てる
  python src/core/nast/build.py --no-wire-hpp  # nast_wire.hpp を作り直さない
  python src/core/nast/build.py --compiler g++ --std c++23

.o は build/obj/ に残して、ソースとヘッダの更新時刻で作り直すか決める。
依存は clang の -MMD が出す .d を読む。コンパイルフラグが変わったら全部捨てる。
parse.cpp などは corpus と wire_test の両方が使うので 1 度だけコンパイルする。

nast_wire.hpp は nodes.json から出た nast_wire.bgn を brgen 本体の
src2json / json2cpp2 に通したもの。tool/ にビルド済みなら毎回作り直し、
無ければ既存のものを残して note を出す (--no-wire-hpp で明示的に落とせる)。

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
import glob
import json
import os
import shutil
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", ".."))
BUILD_DIR = os.path.join(SCRIPT_DIR, "build")
NODEGEN = os.path.join(SCRIPT_DIR, "nodegen.py")
WIREGEN = os.path.join(SCRIPT_DIR, "wiregen.py")
NODES_H = os.path.join(SCRIPT_DIR, "nodes.h")
TEST_CPP = os.path.join(SCRIPT_DIR, "test.cpp")
# corpus はパーサ本体を要る。test.cpp はヘッダだけで足りる。
PARSER_CPP = [os.path.join(SCRIPT_DIR, n) for n in ("parse.cpp", "stream.cpp")]
# ノードの比較。実体を 1 つに閉じてあるので、使う側はリンクするだけ。
COMPARE_CPP = os.path.join(SCRIPT_DIR, "compare.cpp")
# 束縛 / 名前解決。corpus から呼ぶ。
BIND_CPP = sorted(glob.glob(os.path.join(SCRIPT_DIR, "bind", "*.cpp")))
CORPUS_CPP = os.path.join(SCRIPT_DIR, "corpus.cpp")
# LSP 用のダンパ。解析結果を JSON で吐く。
DUMP_CPP = os.path.join(SCRIPT_DIR, "dump.cpp")
# 線上表現の往復。生成物 (nast_wire.hpp / nast_wire_conv.hpp) を要る。
WIRE_CPP = os.path.join(SCRIPT_DIR, "wire_test.cpp")
# 逆変換 (.bgn へ書き戻す) と、その往復検証。
UNPARSE_CPP = os.path.join(SCRIPT_DIR, "unparse.cpp")
UNPARSE_TEST_CPP = os.path.join(SCRIPT_DIR, "unparse_test.cpp")
WIRE_BGN = os.path.join(SCRIPT_DIR, "nast_wire.bgn")
WIRE_HPP = os.path.join(SCRIPT_DIR, "nast_wire.hpp")
# 中間の JSON は追跡しない。
WIRE_JSON = os.path.join(REPO_ROOT, "ignore", "nast", "nast_wire.json")
# nast_wire.hpp を作る brgen 本体のツール。ビルド済みなら使う。
SRC2JSON = os.path.join(REPO_ROOT, "tool", "src2json" + (".exe" if os.name == "nt" else ""))
JSON2CPP2 = os.path.join(REPO_ROOT, "tool", "json2cpp2" + (".exe" if os.name == "nt" else ""))

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


def find_futils_lib(include_dir):
    """<futils>/built/<mode>/<type>/lib/futils.lib を探す"""
    root = os.path.abspath(os.path.join(include_dir, "..", ".."))
    for mode in ("shared", "freestanding-static"):
        for build_type in ("Debug", "Release"):
            for name in ("futils.lib", "libfutils.lib", "libfutils.a"):
                p = os.path.join(root, "built", mode, build_type, "lib", name)
                if os.path.exists(p):
                    return p
    return None


def run(cmd):
    print("$", " ".join(cmd), flush=True)
    result = subprocess.run(cmd)
    if result.returncode != 0:
        sys.exit(result.returncode)


def run_to_file(cmd, out_path):
    """標準出力をファイルへ。src2json も json2cpp2 も結果を stdout に出す。"""
    print("$", " ".join(cmd), ">", out_path, flush=True)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "wb") as f:
        result = subprocess.run(cmd, stdout=f)
    if result.returncode != 0:
        sys.exit(result.returncode)


def regenerate_wire_hpp():
    """nast_wire.bgn から nast_wire.hpp を作り直す。

    futils と同じ規約で「あれば回す、無ければ note」。nodes.h と違って
    brgen 本体のツールが要るので、nast だけを建てたい場合に必須にはしない。
    ただし回さないと nodes.json との対応が黙って古くなる: ノードの増減なら
    conv 側と型が合わなくなって落ちるが、順序だけ変わった場合は気付けない。

    --use-error を付けるのは、失敗が
    "decode: NastModule::strings_len: read int failed" のように
    フィールド名で返るため。
    """
    missing = [p for p in (SRC2JSON, JSON2CPP2) if not os.path.exists(p)]
    if missing:
        print("note: " + ", ".join(os.path.basename(p) for p in missing) +
              " not built; leaving nast_wire.hpp as is")
        return
    run_to_file([SRC2JSON, WIRE_BGN], WIRE_JSON)
    run_to_file([JSON2CPP2, "-f", WIRE_JSON, "--use-error"], WIRE_HPP)


OBJ_DIR = os.path.join(BUILD_DIR, "obj")
# コンパイルフラグが変わったら全部作り直す。フラグは .o に写らないので、
# 中身が古いままリンクされると気付けない。
FLAGS_STAMP = os.path.join(OBJ_DIR, "flags.txt")


def read_deps(dep_path):
    """clang の -MMD が出す make 形式の依存リストを読む。

    Windows のパスは `C:\\...` なので、最初のコロンで切ると
    ドライブレターで切れる。目的ファイルは先頭の 1 語なので、それを落とす。
    パス中の空白はバックスラッシュでエスケープされる。
    """
    if not os.path.exists(dep_path):
        return None
    with open(dep_path, encoding="utf-8", errors="replace") as f:
        text = f.read()
    # 行末のバックスラッシュは継続。改行ごと空白に潰す。
    text = text.replace("\\\r\n", " ").replace("\\\n", " ")

    out = []
    token = ""
    i = 0
    while i < len(text):
        c = text[i]
        if c == "\\" and i + 1 < len(text) and text[i + 1] == " ":
            token += " "
            i += 2
            continue
        if c in " \t\r\n":
            if token:
                out.append(token)
                token = ""
            i += 1
            continue
        token += c
        i += 1
    if token:
        out.append(token)
    if not out:
        return None
    # 先頭は目的ファイル (末尾にコロンが付く)。コロンだけの語で離れることもある。
    out = out[1:] if out[0].endswith(":") else [t for t in out[1:] if t != ":"]
    return out


def obj_is_fresh(obj, dep):
    if not os.path.exists(obj):
        return False
    deps = read_deps(dep)
    if deps is None:
        return False
    t = os.path.getmtime(obj)
    for d in deps:
        try:
            if os.path.getmtime(d) > t:
                return False
        except OSError:
            return False  # 消えたヘッダがあるなら作り直す
    return True


def compile_obj(cmd, src, quiet=False):
    """1 つコンパイルして .o の道を返す。新しければ何もしない。

    build.py はもともと毎回全部コンパイルし直していて、しかも parse.cpp などを
    corpus と wire_test で 2 回コンパイルしていた。-O2 だと 1 回 300 秒近い。
    """
    os.makedirs(OBJ_DIR, exist_ok=True)
    name = os.path.relpath(src, SCRIPT_DIR).replace(os.sep, "_").replace("/", "_")
    obj = os.path.join(OBJ_DIR, name + ".o")
    dep = obj + ".d"
    if obj_is_fresh(obj, dep):
        if not quiet:
            print(f"up to date: {os.path.basename(src)}")
        return obj
    run(cmd + ["-c", src, "-o", obj, "-MMD", "-MF", dep])
    return obj


def flags_changed(cmd):
    """フラグが前回と違えば .o を捨てる。"""
    os.makedirs(OBJ_DIR, exist_ok=True)
    text = " ".join(cmd)
    old = None
    if os.path.exists(FLAGS_STAMP):
        with open(FLAGS_STAMP, encoding="utf-8") as f:
            old = f.read()
    if old == text:
        return False
    for name in os.listdir(OBJ_DIR):
        if name.endswith(".o") or name.endswith(".d"):
            os.remove(os.path.join(OBJ_DIR, name))
    with open(FLAGS_STAMP, "w", encoding="utf-8") as f:
        f.write(text)
    return True


def write_compile_commands(compile_flags):
    """clangd 用の compilation database を出す。

    このディレクトリは CMake を通らないので、置かないと clangd が既定の -std と
    include パスで解釈し、<print> や parse.h が見つからないという誤検出を出す。
    ビルドに使うフラグをそのまま書くので、実際のビルドと食い違わない。
    """
    entries = []
    sources = [os.path.join(SCRIPT_DIR, n) for n in sorted(os.listdir(SCRIPT_DIR))
               if n.endswith(".cpp")]
    sources += BIND_CPP
    sources.append(COMPARE_CPP)
    sources.append(WIRE_CPP)
    for path in sources:
        entries.append({
            "directory": SCRIPT_DIR,
            "file": path,
            "arguments": compile_flags + [path],
        })
    out = os.path.join(SCRIPT_DIR, "compile_commands.json")
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        json.dump(entries, f, indent=2)
        f.write("\n")
    print(f"wrote: {out} ({len(entries)} entries)")


def main():
    parser = argparse.ArgumentParser(description="build nast standalone")
    parser.add_argument("--compiler", help="C++ compiler to use (default: clang++ if available)")
    parser.add_argument("--std", default="c++23", help="language standard (default: c++23)")
    parser.add_argument("--futils", help="futils root directory (overrides autodetection)")
    parser.add_argument("--no-futils", action="store_true", help="skip the as_json check even if futils is available")
    parser.add_argument("--no-generate", action="store_true", help="skip regenerating nodes.h")
    parser.add_argument("--syntax-only", action="store_true", help="compile check only, do not link or run")
    parser.add_argument("--no-run", action="store_true", help="build but do not run the test")
    parser.add_argument("--no-corpus", action="store_true", help="do not build the corpus driver")
    parser.add_argument("--no-wire-hpp", action="store_true",
                        help="do not regenerate nast_wire.hpp with src2json/json2cpp2")
    parser.add_argument("--no-compile-commands", action="store_true",
                        help="do not write compile_commands.json")
    parser.add_argument("-O", "--optimize", default="0", help="optimization level passed as -O<level> (default: 0)")
    args = parser.parse_args()

    if not args.no_generate:
        run([sys.executable, NODEGEN])
        # nast_wire_conv.hpp は nodes.h と同じ扱い。同時に nast_wire.bgn も出るが、
        # そちらは追跡しているので、ずれていれば git の差分として見える。
        run([sys.executable, WIREGEN])
        if not args.no_wire_hpp:
            regenerate_wire_hpp()
    if not os.path.exists(NODES_H):
        sys.exit(f"error: {NODES_H} not found (run without --no-generate)")

    compiler = find_compiler(args.compiler)
    # parse.cpp / corpus.cpp は core/common/file.h を使うので brgen の src も要る。
    # test.cpp には不要だが、足しても害がないのでフラグは 1 本にまとめる
    # (compile_commands.json をファイルごとに分けなくて済む)。
    cmd = [compiler, f"-std={args.std}", f"-O{args.optimize}",
           "-I", SCRIPT_DIR, "-I", os.path.join(REPO_ROOT, "src")]

    link_args = []
    futils_include = None
    if args.no_futils:
        print("note: skipping the as_json check (--no-futils)")
    else:
        futils_include = find_futils(args.futils)
        if futils_include:
            cmd += ["-I", futils_include, "-DNAST_TEST_WITH_JSON"]
            # as_json はヘッダのみで足りるが、JSON のパース (from_json のテスト) は
            # 実体が futils のライブラリにあるのでリンクが要る
            lib = find_futils_lib(futils_include)
            if lib:
                link_args += [lib]
                cmd += ["-DNAST_TEST_WITH_JSON_PARSE"]
                # futils の Debug ビルドとは CRT を揃える必要がある。
                # 揃えないと std::string がライブラリ境界を跨いだ時点でヒープが壊れる。
                if os.name == "nt" and os.sep + "Debug" + os.sep in lib:
                    cmd += ["-fms-runtime-lib=dll_dbg"]
            else:
                print("note: futils library not found; skipping the from_json check")
        else:
            print("note: futils not found; skipping the as_json check "
                  "(set FUTILS_DIR or pass --futils to enable it)")

    if not args.no_compile_commands:
        write_compile_commands(cmd)

    if args.syntax_only:
        run(cmd + ["-fsyntax-only", TEST_CPP])
        print("syntax OK")
        return

    os.makedirs(BUILD_DIR, exist_ok=True)
    if flags_changed(cmd):
        print("note: compile flags changed; rebuilding everything")
    suffix = ".exe" if os.name == "nt" else ""

    exe = os.path.join(BUILD_DIR, "nast_test" + suffix)
    run(cmd + [compile_obj(cmd, TEST_CPP), compile_obj(cmd, COMPARE_CPP), "-o", exe] + link_args)
    print(f"built: {exe}")

    # nodes.h と違って parse.cpp / corpus.cpp は core/common/file.h 経由で
    # futils を実際に使うので、無い場合はビルドできない。
    if args.no_corpus:
        pass
    elif not futils_include:
        print("note: futils not found; skipping the corpus driver")
    else:
        # parse.cpp / stream.cpp / bind/*.cpp は corpus と wire_test の両方が使う。
        # 1 度だけコンパイルして共有する。
        shared = [compile_obj(cmd, src) for src in PARSER_CPP + BIND_CPP + [COMPARE_CPP]]

        corpus = os.path.join(BUILD_DIR, "nast_corpus" + suffix)
        run(cmd + [compile_obj(cmd, CORPUS_CPP)] + shared + ["-o", corpus] + link_args)
        print(f"built: {corpus}")

        dumpexe = os.path.join(BUILD_DIR, "nast_dump" + suffix)
        run(cmd + [compile_obj(cmd, DUMP_CPP)] + shared + ["-o", dumpexe] + link_args)
        print(f"built: {dumpexe}")

        unparse = os.path.join(BUILD_DIR, "nast_unparse_test" + suffix)
        run(cmd + [compile_obj(cmd, UNPARSE_TEST_CPP), compile_obj(cmd, UNPARSE_CPP)] + shared + ["-o", unparse] + link_args)
        print(f"built: {unparse}")

        # 生成物が揃っているときだけ。wiregen.py を回していないと出ない。
        if os.path.exists(os.path.join(SCRIPT_DIR, "nast_wire_conv.hpp")):
            wire = os.path.join(BUILD_DIR, "nast_wire_test" + suffix)
            run(cmd + [compile_obj(cmd, WIRE_CPP)] + shared + ["-o", wire] + link_args)
            print(f"built: {wire}")
        else:
            print("note: nast_wire_conv.hpp not found; run wiregen.py to build the wire driver")

    if not args.no_run:
        run([exe])


if __name__ == "__main__":
    main()
