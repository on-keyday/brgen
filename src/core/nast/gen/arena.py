"""Arena。ノードの実体を型別プールに持ち、Node<T> (headers の添字) で引く。"""

from .schema import Schema
from .writer import Writer


def emit_arena(w: Writer, schema: Schema) -> None:
    w.write("struct Arena {\n")
    w.write("    template<std::uint32_t ord> constexpr auto& get_arena();\n")
    w.write("    template<class T> constexpr auto& get_arena() {  return get_arena<ordinal(get_node_type<T>())>(); }\n")

    # make は型ごとに特殊化しない。get_arena<T>() が既に具象プールへ解決するので
    # 1 つの可変長テンプレートで足りる。抽象ノードは get_arena<T>() の特殊化が無く、
    # コンパイルエラーになる (意図どおり)。
    #   make<T>()               既定構築
    #   make<T>(loc)            loc は NodeHeader へ (NodeData には入らない)
    #   make<T>(loc, a, b, ...) NodeData<T> を集成初期化
    w.write(
        "    template<class T, class... A>\n"
        "    constexpr RefBase<Arena,T> make(lexer::Loc loc = {}, A&&... args) {\n"
        "        auto& pool = get_arena<T>();\n"
        "        pool.push_back(NodeData<T>{std::forward<A>(args)...});\n"
        "        headers.push_back(NodeHeader{.type = get_node_type<T>(),\n"
        "                                     .data_index = static_cast<std::uint32_t>(pool.size() - 1),\n"
        "                                     .loc = loc});\n"
        "        return RefBase<Arena,T>{this,Node<T>{static_cast<std::uint32_t>(headers.size()),get_node_type<T>()}};\n"
        "    }\n"
    )
    w.write("    template<class T> constexpr NodeData<T>* get(Node<T> id);\n")
    w.write("    template<class T> constexpr const NodeData<T>* get(Node<T> id) const;\n")
    w.write("    template<class T> constexpr bool is_valid(Node<T> id) const {\n")
    w.write("        if (id.id() == 0 || headers.size() < id.id()) { return false; }\n")
    w.write("        if (headers[id.id()  - 1].type != id.type()) { return false; }\n")
    w.write("        return true;\n")
    w.write("    }\n")
    w.write("    template<class T> constexpr RefBase<Arena,T> as_ref(Node<T> id)  {\n")
    w.write("        if (!is_valid(id)) {  return {}; }\n")
    w.write("        return RefBase<Arena,T>{this,id};\n")
    w.write("    }\n")
    w.write("    template<class T> constexpr NodeHeader* get_header(Node<T> id) {\n")
    w.write("        if (!is_valid(id)) { return nullptr; }\n")
    w.write("        return &headers[id.id() - 1];\n")
    w.write("    }\n")
    # 型を静的に持たない走査 (pretty printer / from_json) 用の生アクセス
    w.write("    constexpr const NodeHeader* header_at(std::uint32_t id) const {\n")
    w.write("        if (id == 0 || headers.size() < id) { return nullptr; }\n")
    w.write("        return &headers[id - 1];\n")
    w.write("    }\n")
    w.write("    constexpr std::size_t node_count() const { return headers.size(); }\n")
    # from_json は arena を組み立て直すので headers に直接触れる必要がある
    w.write("    constexpr std::vector<NodeHeader>& raw_headers() { return headers; }\n")
    w.write(
        "    template<class T> constexpr NodeData<T>* data_at(std::uint32_t index) {\n"
        "        auto& pool = get_arena<T>();\n"
        "        return index < pool.size() ? &pool[index] : nullptr;\n"
        "    }\n"
    )
    w.write("   private:\n")
    w.write("    std::vector<NodeHeader> headers;\n")
    w.write("   public:\n")

    as_json = Writer()
    as_json.write("    constexpr void as_json(auto&& s) const {\n")
    as_json.write("        auto obj_ = s.object();\n")
    as_json.write('        obj_("headers",headers);\n')

    for node in schema.nodes:
        name = node["name"]
        # 抽象型の Node からも引けるように、全子孫を switch で展開して
        # 具象プールのポインタを基底へアップキャストする。
        for const in ("", "const"):
            w.write("    template<> ", const, " NodeData<", name, ">* get(Node<", name, "> id) ", const, " {\n")
            w.write("        if (!is_valid(id)) { return nullptr; }\n")
            w.write("        switch (id.type()) {\n")
            for concrete in schema.descendants_with_self(node):
                w.write(
                    "        case NodeType::", concrete["name"],
                    ": return &data_", concrete["name"], "[headers[id.id() - 1].data_index];\n",
                )
            w.write("        default: return nullptr;\n")
            w.write("        }\n")
            w.write("    }\n")
        if node.get("abstract"):
            continue
        w.write("   private:\n")
        w.write("    std::vector<NodeData<", name, ">> data_", name, ";\n")
        as_json.write('        obj_("data_', name, '",data_', name, ");\n")
        w.write("   public:\n")
        w.write("    template<>\n")
        w.write("    constexpr auto& get_arena<ordinal(NodeType::", name, ")>() { return data_", name, "; }\n")

    # プールを名前つきで列挙する。from_json はこれで JSON 側と突き合わせる。
    w.write("    constexpr void for_each_pool(auto&& f_) {\n")
    for node in schema.concrete_nodes():
        w.write('        f_("data_', node["name"], '",data_', node["name"], ");\n")
    w.write("    }\n")

    as_json.write("    }")
    w.write(as_json.getvalue())
    w.write("};\n")
    w.write("template<class T> using Ref = RefBase<Arena,T>;\n")
