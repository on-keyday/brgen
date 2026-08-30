#!/usr/bin/env python3
"""バックエンドの雛形を作る。

  python src/core/nast/newbackend.py <lang> [--ext .go]

`src/core/nast/backends/<lang>/` に main.cpp を置き、CMakeLists.txt に
ターゲットを足す。中身は「何も差し込まないバックエンド」で、そのまま建てて
走らせると全ノードが `{{Unhandled node: ...}}` になる。そこから knob を
埋めていく。

rebrgen の script/ebmcodegen.py に当たるが、生成するものは 1 ファイルだけ。
あちらは共有 body (77k 行) を include する薄いラッパを作るのに対し、
nast は knobs.hpp / defaults.hpp をそのまま include すれば済む。

既にあるものは上書きしない (書きかけを消さないため)。
"""

import argparse
import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BACKENDS_DIR = os.path.join(SCRIPT_DIR, "backends")
CMAKE_PATH = os.path.join(SCRIPT_DIR, "CMakeLists.txt")

MAIN_TEMPLATE = '''/*license*/
// {lang} のバックエンド。
//
//   ./nast2{lang} -i input.bgn            標準出力へ
//   ./nast2{lang} -i input.bgn -o out{ext}
//   ./nast2{lang} -i input.bgn --unhandled error   未対応で止める
//
// 木を辿るのは共通側 (backend/entry.hpp)。ここに書くのは knob の設定だけ。
// 何も差し込まなければ全ノードが {{{{Unhandled node: ...}}}} として出るので、
// 出力を見ながら埋めていく。
#include "../../backend/entry.hpp"

using namespace brgen::nast;
using namespace brgen::nast::backend;

struct {config} {{
    static constexpr auto lang_name = "{lang}";
    static constexpr auto file_extension = "{ext}";
    // 言語ごとの状態はここに置く (ctx.lang_config() で取れる)。

    // 言語ごとの追加フラグが要るならここで登録する。--show-flags にも出る。
    // void bind(futils::cmdline::option::Context& ctx) {{
    //     ctx.VarString<true>(&package, "package", "package name", "NAME");
    // }}
}};

NAST_BACKEND_ENTRY({config}) {{
    // knobs.bind_Module(ctx, [](auto& c, Node<Module> n) -> expected<CodeWriter> {{
    //     CodeWriter w;
    //     for (auto& s : n.ref(c.arena())->statements) {{
    //         MAYBE(part, c.visit(s));
    //         w.write(std::move(part));
    //     }}
    //     return w;
    // }});
    //
    // デフォルトの挙動を一部だけ変えたいときは、その場で c.visit_default(n)
    // を呼んで残りを任せる。
    return {{}};
}}
'''

CMAKE_MARKER = "# ---- バックエンド ----"
CMAKE_BLOCK = """
# ---- バックエンド ----
# newbackend.py が足す。ここに並んだものが nast2<lang> として建つ。
"""


def add_to_cmake(lang: str) -> bool:
    """CMakeLists.txt にターゲットを足す。既にあれば何もしない。"""
    text = open(CMAKE_PATH, encoding="utf-8").read()
    target = f"nast2{lang}"
    if re.search(rf"add_executable\({re.escape(target)}\b", text):
        return False
    if CMAKE_MARKER not in text:
        # 計測の節の手前に置く。無ければ末尾。
        anchor = "# ---- 計測 ---"
        if anchor in text:
            text = text.replace(anchor, CMAKE_BLOCK.strip() + "\n\n" + anchor, 1)
        else:
            text += CMAKE_BLOCK
    entry = (
        f'add_executable({target} "backends/{lang}/main.cpp")\n'
        f"target_link_libraries({target} PRIVATE nast_core)\n"
    )
    lines = text.split("\n")
    for i, line in enumerate(lines):
        if line.strip() == CMAKE_MARKER:
            # マーカー直後のコメント行を飛ばして挿す
            j = i + 1
            while j < len(lines) and lines[j].lstrip().startswith("#"):
                j += 1
            lines.insert(j, entry.rstrip("\n"))
            break
    open(CMAKE_PATH, "w", encoding="utf-8", newline="\n").write("\n".join(lines))
    return True


def add_to_dll_list(lang: str) -> bool:
    """Windows の dll コピー対象に足す。"""
    text = open(CMAKE_PATH, encoding="utf-8").read()
    target = f"nast2{lang}"
    if target in text.split("foreach(t nast_test")[-1].split("endforeach")[0]:
        return False
    text = text.replace(
        "foreach(t nast_test nast_corpus nast_dump nast_unparse_test nast_wire_test nast_backend",
        f"foreach(t {target} nast_test nast_corpus nast_dump nast_unparse_test nast_wire_test nast_backend",
        1)
    open(CMAKE_PATH, "w", encoding="utf-8", newline="\n").write(text)
    return True


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("lang", help="language name (used as nast2<lang>)")
    p.add_argument("--ext", default="", help="file extension for the example in comments")
    args = p.parse_args()

    lang = args.lang
    if not re.fullmatch(r"[a-z][a-z0-9_]*", lang):
        sys.exit(f"error: language name must be lowercase identifier: {lang}")
    ext = args.ext or f".{lang}"

    out_dir = os.path.join(BACKENDS_DIR, lang)
    main_cpp = os.path.join(out_dir, "main.cpp")
    if os.path.exists(main_cpp):
        print(f"exists, kept as is: {main_cpp}")
    else:
        os.makedirs(out_dir, exist_ok=True)
        config = "".join(w.capitalize() for w in lang.split("_")) + "Config"
        text = MAIN_TEMPLATE.format(lang=lang, ext=ext, config=config)
        open(main_cpp, "w", encoding="utf-8", newline="\n").write(text)
        print(f"wrote: {main_cpp}")

    if add_to_cmake(lang):
        print(f"added target nast2{lang} to CMakeLists.txt")
    else:
        print(f"target nast2{lang} already in CMakeLists.txt")
    if add_to_dll_list(lang):
        print("added to the runtime dll list")

    print()
    print("next:")
    print(f"  python src/core/nast/build.py -t nast2{lang} --no-run")
    print(f"  ./src/core/nast/build/fast/bin/nast2{lang} -i example/udp.bgn")
    return 0


if __name__ == "__main__":
    sys.exit(main())
