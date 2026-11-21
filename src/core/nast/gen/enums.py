"""enum の定義、ast::enum_array<T> 相当のヘルパ、演算子の優先順位層。"""

import json

from .schema import Schema
from .writer import Writer


def emit_enums(w: Writer, schema: Schema) -> None:
    for enum in schema.enums:
        max_alt_names = max(len(x.get("alt_names", [])) for x in enum["enums"])
        w.write("enum class ", enum["name"], "{\n")
        for elem in enum["enums"]:
            w.write(
                "    ",
                elem["name"],
                " = " + elem["value"] if "value" in elem else "",
                ",\n",
            )
        w.write("};\n")
        w.write(
            "constexpr const char* to_string(",
            enum["name"],
            " t",
            ", size_t alt = 0" if max_alt_names > 0 else "",
            ") {\n",
        )
        w.write("    switch (t) {\n")
        for elem in enum["enums"]:
            w.write("    case ", enum["name"], "::", elem["name"], ":\n")
            alt_names = elem.get("alt_names", [])
            if alt_names:
                w.write("        switch (alt) {\n")
                for i, alt in enumerate(alt_names):
                    w.write("        case ", str(i + 1), ": return ", json.dumps(alt), ";\n")
                w.write("        }\n")
            w.write('        return "', elem["name"], '";\n')
        w.write("    }\n")
        w.write("    return nullptr;\n")
        w.write("}\n")
        w.write("constexpr void as_json(", enum["name"], " t,auto&& s) {\n")
        w.write("    s.string(to_string(t));\n")
        w.write("}\n")


# ast 側 (src/core/ast/node/ast_enum.h, enum_gen が生成) と同じ一式を出す。
#   enum_array<T>      : 値 -> 代表表記 (alt_names[0]、無ければ name)
#   enum_name_array<T> : 値 -> name
#   from_string<T>     : name と alt_names の両方を受ける
# ast の from_string は代表表記しか受けないが、nast の as_json は name を出すので、
# それだと round-trip しない。ここは意図的に superset にしてある。
HELPER_DECLS = (
    "template<class T> constexpr std::optional<T> from_string(std::string_view str);\n"
    "template<class T> constexpr std::size_t enum_elem_count();\n"
    "template<class T> constexpr std::array<std::pair<T,std::string_view>,enum_elem_count<T>()> make_enum_array();\n"
    "template<class T> constexpr std::array<std::pair<T,std::string_view>,enum_elem_count<T>()> make_enum_name_array();\n"
    "template<class T> constexpr const char* enum_type_name();\n"
    "template<class T> constexpr auto enum_array = make_enum_array<T>();\n"
    "template<class T> constexpr auto enum_name_array = make_enum_name_array<T>();\n"
)


def emit_enum_helper_decls(w: Writer) -> None:
    w.write(HELPER_DECLS)


def emit_enum_helpers(w: Writer, type_name: str, members: list) -> None:
    """members: [(cpp_name, name, alt_names)]"""
    count = len(members)
    sig = f"std::array<std::pair<{type_name},std::string_view>,{count}>"

    w.write(f"template<> constexpr std::size_t enum_elem_count<{type_name}>() {{ return {count}; }}\n")

    for fn, pick in (
        ("make_enum_array", lambda n, a: a[0] if a else n),
        ("make_enum_name_array", lambda n, a: n),
    ):
        w.write(f"template<> constexpr {sig} {fn}<{type_name}>() {{\n    return {{\n")
        for cpp, name, alts in members:
            w.write(f"        std::pair{{{type_name}::{cpp},{json.dumps(pick(name, alts))}}},\n")
        w.write("    };\n}\n")

    w.write(f"template<> constexpr std::optional<{type_name}> from_string<{type_name}>(std::string_view str) {{\n")
    w.write("    if (str.empty()) { return std::nullopt; }\n")
    for cpp, name, alts in members:
        seen = []
        for s in [name] + list(alts):
            if s not in seen:
                seen.append(s)
                w.write(f"    if (str == {json.dumps(s)}) {{ return {type_name}::{cpp}; }}\n")
    w.write("    return std::nullopt;\n}\n")

    w.write(f'template<> constexpr const char* enum_type_name<{type_name}>() {{ return "{type_name}"; }}\n')


def emit_expr_layers(w: Writer, schema: Schema, enum: dict) -> None:
    """演算子の優先順位層 (ast::expr_layer.h 相当)。

    層 -> 平坦化した enum、という向きで生成するので両者がズレない。
    述語の範囲も生成時に順序を正規化するため、ast 側の is_range_op のように
    begin > end で常に false になる事故が起きない。
    """
    layers = enum.get("layers")
    if not layers:
        return
    type_name = enum["name"]
    prefix = enum.get("layer_prefix", "bin")
    order = {m["name"]: i for i, m in enumerate(enum["enums"])}

    w.write(f"// ---- {type_name} operator precedence layers ----\n")
    w.write("namespace layer_detail {\n")
    for layer in layers:
        arr = ",".join(json.dumps(Schema.primary_name(m)) for m in layer["enums"])
        w.write(f"    constexpr std::string_view {prefix}_layer_{layer['name']}[] = {{{arr}}};\n")
    w.write("}\n")

    spans = ", ".join(
        f"std::span<const std::string_view>{{layer_detail::{prefix}_layer_{l['name']}}}"
        for l in layers
    )
    w.write(
        f"constexpr std::array<std::span<const std::string_view>,{len(layers)}> "
        f"{prefix}_layers = {{{spans}}};\n"
    )
    # ignored な層はパーサの層走査の対象外なので、長さを 2 種類出す
    active = [l for l in layers if not l.get("ignored")]
    w.write(f"constexpr std::size_t {prefix}_layer_count = {len(layers)};\n")
    w.write(f"constexpr std::size_t {prefix}_layer_len = {len(active)};\n")
    for i, layer in enumerate(layers):
        w.write(f"constexpr std::size_t {prefix}_{layer['name']}_layer = {i};\n")

    for g in enum.get("groups", []):
        conds = []
        if "from" in g and "to" in g:
            # 生成時に順序を正規化する。schema 上の from/to が逆でも範囲が壊れない。
            lo, hi = g["from"], g["to"]
            if order[lo] > order[hi]:
                lo, hi = hi, lo
            conds.append(f"(int({type_name}::{lo}) <= int(op) && int(op) <= int({type_name}::{hi}))")
        for m in list(g.get("members", [])) + list(g.get("extra", [])):
            conds.append(f"op == {type_name}::{m}")
        w.write(
            f"constexpr bool is_{g['name']}({type_name} op) {{\n"
            f"    return {' || '.join(conds)};\n"
            "}\n"
        )
