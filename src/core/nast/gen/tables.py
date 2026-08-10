"""ノードではない素の値型と、解析結果を置く side table。"""

from .nodes import emit_fields
from .schema import Schema
from .writer import Writer

STORAGE_KINDS = ("dense", "sparse", "flag")


def emit_structs(w: Writer, schema: Schema) -> None:
    """side table の値や NodeData のフィールドから使う値型。

    NodeType にも arena にも入らない。
    """
    for struct in schema.structs:
        w.write("struct ", struct["name"], " {\n")
        emit_fields(w, schema, struct)
        w.write("};\n")


def emit_side_tables(w: Writer, schema: Schema) -> None:
    """解析結果をノードの外に置く表。

    キーは Node::id() (headers の添字) で統一し、密度に応じて storage を選ぶ。
    API は storage によらず contains/get/set/erase で共通なので、密度の見積もりを
    外しても宣言を 1 語変えて再生成すれば呼び出し側は無変更で済む。
    """
    for t in schema.side_tables:
        _emit_one(w, schema, t)
    _emit_aggregate(w, schema)


def _emit_one(w: Writer, schema: Schema, t: dict) -> None:
    name = t["name"]
    over = t["over"]
    storage = t.get("storage", "sparse")
    if storage not in STORAGE_KINDS:
        raise ValueError(f"unknown storage for side table {name}: {storage}")
    schema.node_by_name(over)  # 未知のノード型なら KeyError で落とす
    table = name + "Table"
    node_t = f"Node<{over}>"

    # flag は値を持たないが、table<T>() のキーとして使うためタグ型は出す。
    w.write("struct ", name, " {\n")
    emit_fields(w, schema, t if storage != "flag" else {"fields": []})
    w.write("};\n")

    w.write(f"// side table over {over} (storage: {storage})\n")
    w.write(f"struct {table} {{\n")
    w.write(f"    using node_type = {over};\n")
    w.write(f"    using key_type = {name};\n")
    if storage != "flag":
        w.write(f"    using value_type = {name};\n")

    {"dense": _emit_dense, "sparse": _emit_sparse, "flag": _emit_flag}[storage](w, table, node_t)
    w.write("};\n")


def _emit_dense(w: Writer, table: str, node_t: str) -> None:
    w.write(
        "   private:\n"
        "    std::vector<value_type> values_;\n"
        "    std::vector<bool> present_;\n"
        "    void grow_(std::uint32_t id) {\n"
        "        if (values_.size() <= id) { values_.resize(id + 1); present_.resize(id + 1, false); }\n"
        "    }\n"
        "   public:\n"
        f"    bool contains({node_t} n) const {{\n"
        "        auto i = n.id();\n"
        "        return i != 0 && i < present_.size() && present_[i];\n"
        "    }\n"
        f"    value_type* get({node_t} n) {{ return contains(n) ? &values_[n.id()] : nullptr; }}\n"
        f"    const value_type* get({node_t} n) const {{ return contains(n) ? &values_[n.id()] : nullptr; }}\n"
        f"    value_type* set({node_t} n, value_type v) {{\n"
        "        if (n.is_null()) { return nullptr; }\n"
        "        grow_(n.id());\n"
        "        present_[n.id()] = true;\n"
        "        values_[n.id()] = std::move(v);\n"
        "        return &values_[n.id()];\n"
        "    }\n"
        f"    bool erase({node_t} n) {{\n"
        "        if (!contains(n)) { return false; }\n"
        "        present_[n.id()] = false;\n"
        "        values_[n.id()] = value_type{};\n"
        "        return true;\n"
        "    }\n"
        "    std::size_t size() const {\n"
        "        std::size_t n = 0;\n"
        "        for (std::size_t i = 0; i < present_.size(); i++) { if (present_[i]) { n++; } }\n"
        "        return n;\n"
        "    }\n"
        "    void as_json(auto&& s) const {\n"
        "        auto obj_ = s.object();\n"
        "        std::string key;\n"
        "        for (std::uint32_t i = 1; i < present_.size(); i++) {\n"
        "            if (!present_[i]) { continue; }\n"
        "            key = std::to_string(i);\n"
        "            obj_(key.c_str(), values_[i]);\n"
        "        }\n"
        "    }\n"
    )


def _emit_sparse(w: Writer, table: str, node_t: str) -> None:
    w.write(
        "   public:\n"
        "    struct Entry {\n"
        "        std::uint32_t node{};\n"
        "        value_type value{};\n"
        "        constexpr void as_json(auto&& s) const {\n"
        "            auto obj_ = s.object();\n"
        '            obj_("node",node);\n'
        '            obj_("value",value);\n'
        "        }\n"
        "    };\n"
        "   private:\n"
        "    std::vector<Entry> entries_;  // node 昇順\n"
        "    static constexpr auto by_node_ = [](const Entry& e, std::uint32_t v) { return e.node < v; };\n"
        "    auto find_(std::uint32_t id) const { return std::lower_bound(entries_.begin(), entries_.end(), id, by_node_); }\n"
        "    auto find_(std::uint32_t id) { return std::lower_bound(entries_.begin(), entries_.end(), id, by_node_); }\n"
        "   public:\n"
        f"    bool contains({node_t} n) const {{\n"
        "        if (n.is_null()) { return false; }\n"
        "        auto it = find_(n.id());\n"
        "        return it != entries_.end() && it->node == n.id();\n"
        "    }\n"
        f"    const value_type* get({node_t} n) const {{\n"
        "        if (n.is_null()) { return nullptr; }\n"
        "        auto it = find_(n.id());\n"
        "        return (it != entries_.end() && it->node == n.id()) ? &it->value : nullptr;\n"
        "    }\n"
        f"    value_type* get({node_t} n) {{\n"
        f"        return const_cast<value_type*>(static_cast<const {table}*>(this)->get(n));\n"
        "    }\n"
        f"    value_type* set({node_t} n, value_type v) {{\n"
        "        if (n.is_null()) { return nullptr; }\n"
        "        // 構築は id 昇順になることが多いので、その場合は push_back で済む\n"
        "        if (entries_.empty() || entries_.back().node < n.id()) {\n"
        "            entries_.push_back(Entry{n.id(), std::move(v)});\n"
        "            return &entries_.back().value;\n"
        "        }\n"
        "        auto it = find_(n.id());\n"
        "        if (it != entries_.end() && it->node == n.id()) {\n"
        "            it->value = std::move(v);\n"
        "            return &it->value;\n"
        "        }\n"
        "        it = entries_.insert(it, Entry{n.id(), std::move(v)});\n"
        "        return &it->value;\n"
        "    }\n"
        f"    bool erase({node_t} n) {{\n"
        "        if (n.is_null()) { return false; }\n"
        "        auto it = find_(n.id());\n"
        "        if (it == entries_.end() || it->node != n.id()) { return false; }\n"
        "        entries_.erase(it);\n"
        "        return true;\n"
        "    }\n"
        "    std::size_t size() const { return entries_.size(); }\n"
        "    const std::vector<Entry>& entries() const { return entries_; }\n"
        "    void as_json(auto&& s) const {\n"
        "        auto obj_ = s.object();\n"
        "        std::string key;\n"
        "        for (auto& e : entries_) {\n"
        "            key = std::to_string(e.node);\n"
        "            obj_(key.c_str(), e.value);\n"
        "        }\n"
        "    }\n"
    )


def _emit_flag(w: Writer, table: str, node_t: str) -> None:
    w.write(
        "   private:\n"
        "    std::vector<std::uint32_t> nodes_;  // 昇順\n"
        "    auto find_(std::uint32_t id) const {\n"
        "        return std::lower_bound(nodes_.begin(), nodes_.end(), id);\n"
        "    }\n"
        "   public:\n"
        f"    bool contains({node_t} n) const {{\n"
        "        if (n.is_null()) { return false; }\n"
        "        auto it = find_(n.id());\n"
        "        return it != nodes_.end() && *it == n.id();\n"
        "    }\n"
        f"    bool set({node_t} n) {{\n"
        "        if (n.is_null() || contains(n)) { return false; }\n"
        "        if (nodes_.empty() || nodes_.back() < n.id()) { nodes_.push_back(n.id()); return true; }\n"
        "        nodes_.insert(find_(n.id()), n.id());\n"
        "        return true;\n"
        "    }\n"
        f"    bool erase({node_t} n) {{\n"
        "        if (!contains(n)) { return false; }\n"
        "        nodes_.erase(find_(n.id()));\n"
        "        return true;\n"
        "    }\n"
        "    std::size_t size() const { return nodes_.size(); }\n"
        "    const std::vector<std::uint32_t>& nodes() const { return nodes_; }\n"
        "    void as_json(auto&& s) const {\n"
        "        auto obj_ = s.object();\n"
        "        std::string key;\n"
        "        for (auto id : nodes_) {\n"
        "            key = std::to_string(id);\n"
        "            obj_(key.c_str(), true);\n"
        "        }\n"
        "    }\n"
    )


def _emit_aggregate(w: Writer, schema: Schema) -> None:
    """全 side table を集める入れ物。

    Arena に持たせると「持ち回す入れ物に構文と解析結果が同居する」形になり、
    side table を分けた意味が 1 段上で失われるので、構文木とは別の入れ物にする。
    """
    if not schema.side_tables:
        return
    names = [t["name"] for t in schema.side_tables]
    w.write("struct SideTables {\n")
    w.write("    template<class T> constexpr auto& table();\n")
    w.write("    template<class T> constexpr const auto& table() const;\n")
    for name in names:
        w.write("   private:\n")
        w.write(f"    {name}Table {name}_;\n")
        w.write("   public:\n")
        w.write(f"    template<> constexpr auto& table<{name}>() {{ return {name}_; }}\n")
        w.write(f"    template<> constexpr const auto& table<{name}>() const {{ return {name}_; }}\n")
    # 表を名前つきで列挙する。Arena::for_each_pool と同じ役目で、
    # どの表があるかを静的に知らない側 (printer など) が回すのに要る。
    w.write("    constexpr void for_each_table(auto&& f_) const {\n")
    for name in names:
        w.write(f'        f_("{name}",{name}_);\n')
    w.write("    }\n")
    w.write("    void as_json(auto&& s) const {\n")
    w.write("        auto obj_ = s.object();\n")
    for name in names:
        w.write(f'        obj_("{name}",{name}_);\n')
    w.write("    }\n")
    w.write("};\n")
