"""フィールド名からメンバを引く対応表。access.h のパス走査がこれに乗る。

nodes.h の for_each_field は名前を実行時に渡すので、名前からメンバの「型」を選べない。
ここでは型ごとに FieldOf<T, "name"> を出して、コンパイル時に選べるようにする。
継承したフィールドも対象にするので、NodeData<Format> から "belong" も引ける。
"""

from .schema import Schema
from .writer import Writer


def emit_fixed_string(w: Writer) -> None:
    """Node/Ref の field<"..."> がこれを非型引数に取るので、Node より前に出す。"""
    # 文字列リテラルを非型テンプレート引数にするための入れ物。
    # 構造体型 (公開メンバのみ) なので auto NTTP に渡せる。
    w.write(
        "template<std::size_t N>\n"
        "struct fixed_string {\n"
        "    char value[N]{};\n"
        "    constexpr fixed_string() = default;\n"
        "    consteval fixed_string(const char (&s)[N]) {\n"
        "        for (std::size_t i = 0; i < N; i++) { value[i] = s[i]; }\n"
        "    }\n"
        "    constexpr std::string_view view() const { return std::string_view(value, N - 1); }\n"
        "    constexpr bool operator==(const fixed_string&) const = default;\n"
        "};\n"
        "template<std::size_t N> fixed_string(const char (&)[N]) -> fixed_string<N>;\n"
    )

    # パス走査の実体は access.h。Node/Ref から呼べるように宣言だけ先に置く。
    w.write("template<class T> struct Node;\n")
    w.write("template<fixed_string Path, class A, class T> constexpr auto node_field(A& a, Node<T> id);\n")


def emit_field_of(w: Writer, schema: Schema) -> None:
    """フィールド名からメンバを引く対応表。NodeData が出た後でないと書けない。"""
    # 一次テンプレートは定義しない。存在しないフィールド名を書いたら
    # 「不完全型」としてコンパイルエラーになるのが狙い。
    w.write("template<class T, auto Name> struct FieldOf;\n")

    for node in schema.nodes:
        name = node["name"]
        # all_fields は基底から順に出すので、同名フィールドは後ろ (派生側) が勝つ。
        # C++ 側でも派生のメンバが基底のを隠すので、これで d_.<name> と一致する。
        # 例: Available は Expr::type を隠す type を自前で持つ。
        by_name = {}
        for field in schema.all_fields(node):
            by_name[field["name"]] = field
        for fname in by_name:
            w.write(
                "template<> struct FieldOf<", name, ", fixed_string(\"", fname, "\")> {\n",
                "    static constexpr auto& get(NodeData<", name, ">& d_) { return d_.", fname, "; }\n",
                "    static constexpr const auto& get(const NodeData<", name, ">& d_) { return d_.", fname, "; }\n",
                "};\n",
            )
