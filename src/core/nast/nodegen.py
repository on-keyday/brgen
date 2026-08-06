import json
import io
import os

# CWD ではなくこのスクリプトの位置を基準にする (どこから呼んでも動くように)
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NODES_JSON = os.path.join(SCRIPT_DIR, "nodes.json")
NODES_H = os.path.join(SCRIPT_DIR, "nodes.h")

with open(NODES_JSON, encoding="utf-8") as f:
    node_def = json.load(f)


class Writer:
    def __init__(self):
        self.w = io.StringIO()

    def write(self, *args):
        [self.w.write(x) for x in args]

    def getvalue(self):
        return self.w.getvalue()


incl_def = Writer()
enum_def = Writer()
constexpr_fn_def = Writer()
header_def = Writer()
fwd_def = Writer()
body = Writer()

arena = Writer()

includes = node_def["includes"]
types = node_def["types"]
enums = node_def["enums"]
nodes = node_def["nodes"]
# ノードではない素の値型と、解析結果を置く side table。どちらも無くてもよい。
structs = node_def.get("structs", [])
side_tables = node_def.get("side_tables", [])

[incl_def.write("#include ", x, "\n") for x in includes]

for enum in enums:
    max_alt_names = max([len(x.get("alt_names", [])) for x in enum["enums"]])
    enum_def.write("enum class ", enum["name"], "{\n")
    for elem in enum["enums"]:
        enum_def.write(
            "    ",
            elem["name"],
            " = " + elem["value"] if "value" in elem else "",
            ",\n",
        )
    enum_def.write("};\n")
    enum_def.write(
        "constexpr const char* to_string(",
        enum["name"],
        " t",
        ", size_t alt = 0" if max_alt_names > 0 else "",
        ") {\n",
    )
    enum_def.write("    switch (t) {\n")
    for elem in enum["enums"]:
        enum_def.write("    case ", enum["name"], "::", elem["name"], ":\n")
        alt_names = elem.get("alt_names", [])
        if len(alt_names) > 0:
            enum_def.write("        switch (alt) {\n")
            for i, alt in enumerate(alt_names):
                enum_def.write(
                    "        case ", str(i + 1), ": return ", json.dumps(alt), ";\n"
                )
            enum_def.write("        }\n")
        enum_def.write('        return "', elem["name"], '";\n')
    enum_def.write("    }\n")
    enum_def.write("    return nullptr;\n")
    enum_def.write("}\n")
    enum_def.write("constexpr void as_json(", enum["name"], " t,auto&& s) {\n")
    enum_def.write("    s.string(to_string(t));\n")
    enum_def.write("}\n")


# ---- enum のヘルパ群 (ast::enum_array<T> 相当) ---------------------------------
# ast 側 (src/core/ast/node/ast_enum.h, enum_gen が生成) と同じ一式を出す。
#   enum_array<T>      : 値 -> 代表表記 (alt_names[0]、無ければ name)
#   enum_name_array<T> : 値 -> name
#   from_string<T>     : name と alt_names の両方を受ける
# ast の from_string は代表表記しか受けないが、nast の as_json は name を出すので、
# それだと round-trip しない。ここは意図的に superset にしてある。
enum_helper_def = Writer()
enum_helper_def.write(
    "template<class T> constexpr std::optional<T> from_string(std::string_view str);\n"
    "template<class T> constexpr std::size_t enum_elem_count();\n"
    "template<class T> constexpr std::array<std::pair<T,std::string_view>,enum_elem_count<T>()> make_enum_array();\n"
    "template<class T> constexpr std::array<std::pair<T,std::string_view>,enum_elem_count<T>()> make_enum_name_array();\n"
    "template<class T> constexpr const char* enum_type_name();\n"
    "template<class T> constexpr auto enum_array = make_enum_array<T>();\n"
    "template<class T> constexpr auto enum_name_array = make_enum_name_array<T>();\n"
)


def emit_enum_helpers(type_name: str, members: list):
    """members: [(cpp_name, name, alt_names)] を受けてヘルパ一式を出す"""
    w = enum_helper_def
    count = len(members)
    sig = f"std::array<std::pair<{type_name},std::string_view>,{count}>"

    w.write(
        f"template<> constexpr std::size_t enum_elem_count<{type_name}>() {{ return {count}; }}\n"
    )

    for fn, pick in (("make_enum_array", lambda n, a: a[0] if a else n),
                     ("make_enum_name_array", lambda n, a: n)):
        w.write(f"template<> constexpr {sig} {fn}<{type_name}>() {{\n    return {{\n")
        for cpp, name, alts in members:
            w.write(f"        std::pair{{{type_name}::{cpp},{json.dumps(pick(name, alts))}}},\n")
        w.write("    };\n}\n")

    w.write(
        f"template<> constexpr std::optional<{type_name}> from_string<{type_name}>(std::string_view str) {{\n"
    )
    w.write("    if (str.empty()) { return std::nullopt; }\n")
    for cpp, name, alts in members:
        seen = []
        for s in [name] + list(alts):
            if s not in seen:
                seen.append(s)
                w.write(f"    if (str == {json.dumps(s)}) {{ return {type_name}::{cpp}; }}\n")
    w.write("    return std::nullopt;\n}\n")

    w.write(
        f'template<> constexpr const char* enum_type_name<{type_name}>() {{ return "{type_name}"; }}\n'
    )


def resolve_type_name(t: str):
    return (
        t.replace("vector<", "std::vector<")
        .replace("string", "std::string")
        .replace("uint32", "std::uint32_t")
    )


def print_field(body: Writer, node: dict):
    fields = node["fields"]
    if node.get("derive") is not None:
        print_field(body, [n for n in nodes if n["name"] == node["derive"]][0])
    for field in fields:
        # ローカル名は obj_ にしてある。field という名前のフィールドを持つノード
        # (UnionCandidate::field) があり、field だとローカルが実体を隠すため。
        body.write(
            "        obj_(", json.dumps(field["name"]), ",", field["name"], ");\n"
        )


def print_node_field(body: Writer, node: dict):
    fields = node["fields"]
    for field in fields:
        field_name = field["name"]
        field_type = resolve_type_name(field["type"])
        init = field.get("init", types.get(field_type, {}).get("init", ""))
        body.write("    ", field_type, " ", field_name, init, ";\n")
    body.write("    constexpr void as_json(auto&& s) const {\n")
    body.write("        auto obj_ = s.object();\n")
    print_field(body, node)
    body.write("    }\n")


header_def.write("struct NodeHeader {\n")
print_node_field(header_def, node_def["header"])
header_def.write("};\n")
header_def.write("template <class T>\n")
header_def.write("struct Node {\n")
header_def.write("    friend struct Arena;\n")
header_def.write("    template<class> friend struct Node;")
header_def.write("    constexpr Node() : id_{0}, type_{get_node_type<T>()} {}\n")
header_def.write("   private:\n")
header_def.write("    std::uint32_t id_{};\n")
header_def.write("    NodeType type_{};\n")
header_def.write(
    "    constexpr Node(std::uint32_t id,NodeType type) : id_{id},type_{type} {}\n"
)
header_def.write("   public:\n")
header_def.write(
    "    template<std::derived_from<T> U>  constexpr Node(Node<U> x) : id_{x.id_},type_{x.type_} {}\n"
)
header_def.write("    constexpr Node(nullref_t) : Node() {}\n")
# 比較は id_ のみで行う。= default にすると type_ も比べてしまい、
# 既定構築した Node<Format>{} と Node<Function>{} が (どちらも null なのに) 不一致になる。
# id_ は headers の添字なのでノードを一意に決める。
# friend にしてあるので、派生 -> 基底の暗黙変換と nullref_t からの変換が両辺に効く。
#   n == nullref / Node<Format> == Node<Statement> は通り、
#   Node<Format> == Node<Function> のような無関係な比較はコンパイルエラーになる。
header_def.write(
    "    friend constexpr bool operator==(const Node& l, const Node& r) { return l.id_ == r.id_; }\n"
    "    friend constexpr auto operator<=>(const Node& l, const Node& r) { return l.id_ <=> r.id_; }\n"
)
header_def.write("    constexpr NodeType type() const { return type_; }\n")
header_def.write("    constexpr std::uint32_t id() const { return id_; }\n")
header_def.write("    constexpr bool is_null() const { return id_ == 0; }")
header_def.write("    constexpr explicit operator bool() const { return !is_null(); }")
header_def.write(
    "    constexpr std::uint64_t unique_id() const { return (std::uint64_t(type_) << 32) | id_; }\n"
)
header_def.write(
    "    constexpr void as_json(auto&& s) const { s.number(unique_id()); }\n"
)
header_def.write(
    "    template<class A> constexpr auto ref(A& a) { return a.as_ref(*this); }\n"
)
# U は T の派生なので、これは基底 -> 派生 のチェック付きダウンキャスト。
# 既存 AST の ast::as<T>(node) と同じ意味なので名前を合わせる。
header_def.write(
    "    template<class U> requires std::derived_from<U,T> constexpr Node<U> as() const { return is_derived<U>(type_) ? Node<U>{id_,type_} : Node<U>{};  }\n"
)
header_def.write("};\n")
header_def.write("template <class T>\n")
header_def.write("struct NodeData;\n")
enum_def.write("enum class NodeType : std::uint32_t {\n")

derives: dict[str, dict] = {}

for node in nodes:
    if "derive" in node:
        l = derives.get(node["derive"], {"bit": 0, "list": []})
        l["list"].append(node["name"])
        derives[node["derive"]] = l

i = 0
for derive in derives:
    derives[derive]["bit"] = 1 << i
    i += 1
print(derives)


def get_derive_bit(node: dict):
    bit = 0
    if node["name"] in derives:
        bit |= derives[node["name"]]["bit"]
    if "derive" in node:
        bit |= derives[node["derive"]]["bit"]
        bit |= get_derive_bit([n for n in nodes if n["name"] == node["derive"]][0])
    return bit


for i, node in enumerate(nodes):
    name = node["name"]
    enum_def.write(
        "    ",
        name,
        " = (",
        str(i),
        " << ",
        str(len(derives)),
        ") | ",
        str(get_derive_bit(node)),
        ",\n",
    )
    constexpr_fn_def.write(
        "template<> constexpr NodeType get_node_type<",
        name,
        ">() { return NodeType::",
        name,
        "; }\n",
    )
    fwd_def.write(
        "struct ", name, ":" + node["derive"] if "derive" in node else "", "{};\n"
    )
    body.write("template<>\n")
    body.write(
        "struct NodeData<",
        name,
        ">",
        (": NodeData<" + node["derive"] + ">" if "derive" in node else ""),
        " {\n",
    )
    print_node_field(body, node)
    body.write("};\n")

enum_def.write("};\n")
enum_def.write("template<class T> constexpr NodeType get_node_type();\n")
enum_def.write("template<class T> constexpr bool is_derived(NodeType);\n")
enum_def.write(
    "constexpr std::uint32_t ordinal(NodeType t) { return std::uint32_t(t) >> ",
    str(len(derives)),
    "; }\n",
)
enum_def.write("constexpr const char* to_string(NodeType t) {\n")
enum_def.write("    switch(t) {")
for i, node in enumerate(nodes):
    name = node["name"]
    enum_def.write("    case NodeType::", name, ': return "', name, '";\n')
enum_def.write("    default: return nullptr;")
enum_def.write("    }\n")
enum_def.write("}\n")
enum_def.write(
    "constexpr void as_json(NodeType t,auto&& s) { s.string(to_string(t)); }"
)


for node in nodes:
    if node["name"] in derives:
        enum_def.write(
            "template<> constexpr bool is_derived<",
            node["name"],
            ">(NodeType t) { return std::uint32_t(t) & ",
            str(derives[node["name"]]["bit"]),
            "; }\n",
        )
    else:
        enum_def.write(
            "template<> constexpr bool is_derived<",
            node["name"],
            ">(NodeType t) { return t == NodeType::",
            node["name"],
            "; }\n",
        )


enum_def.write("struct Arena;\n")
enum_def.write("struct nullref_t {};\n")
enum_def.write("constexpr nullref_t nullref{};\n")
header_def.write("template<class A,class T>\n")
header_def.write("struct Ref {\n")
header_def.write("    friend struct Arena;\n")
header_def.write("    constexpr Ref() = default;\n")
header_def.write("   private:\n")
header_def.write("    A* arena_ = nullptr;\n")
header_def.write("    Node<T> id_;\n")
header_def.write("    constexpr Ref(A* a,Node<T> id): arena_{a},id_{id} {}\n")
header_def.write("   public:\n")
header_def.write("    constexpr Ref(nullref_t) : arena_{nullptr},id_{} {}\n")
header_def.write("    constexpr Node<T> id() const {  return id_; }\n")
header_def.write("    constexpr A* arena() { return arena_; }\n")
# Node<T> だけでなく基底の Node<U> へも直接変換できるようにする。
# operator Node<T> のみだと Ref -> Node<T> -> Node<U> がユーザー定義変換 2 段になり、
# vector<Node<Statement>>::push_back(Ref<Arena,Field>) のような呼び出しが通らない。
header_def.write("    template<class U> requires std::derived_from<T,U>\n")
header_def.write("    constexpr operator Node<U>() const  { return id_; }\n")
header_def.write(
    "    constexpr NodeData<T>* get() { return arena_ ? arena_->get(id_) : nullptr; }\n"
)
header_def.write("    constexpr NodeData<T>* operator->() { return get(); }\n")
# loc は NodeData ではなく NodeHeader にあるので、payload 経由では届かない。
# 旧 AST の node->loc に相当する経路をここで用意する。
header_def.write(
    "    constexpr lexer::Loc loc() const {\n"
    "        auto* h = arena_ ? arena_->get_header(id_) : nullptr;\n"
    "        return h ? h->loc : lexer::Loc{};\n"
    "    }\n"
    "    constexpr void set_loc(lexer::Loc l) {\n"
    "        if (auto* h = arena_ ? arena_->get_header(id_) : nullptr) { h->loc = l; }\n"
    "    }\n"
)
header_def.write("    constexpr NodeData<T>& operator*() { return *get(); }\n")
header_def.write(
    "    constexpr explicit operator bool() const { return arena_ && !id_.is_null(); }\n"
)
header_def.write(
    "    constexpr NodeHeader* header() { return arena_ ? arena_->get_header(id_) : nullptr; }\n"
)
header_def.write("};\n")

arena.write("struct Arena {\n")
arena.write("    template<std::uint32_t ord> constexpr auto& get_arena();\n")
arena.write(
    "    template<class T> constexpr auto& get_arena() {  return get_arena<ordinal(get_node_type<T>())>(); }\n"
)
# make は型ごとに特殊化しない。get_arena<T>() が既に具象プールへ解決するので、
# 1 つの可変長テンプレートで足りる。
#   make<T>()                       -> 既定構築
#   make<T>(loc)                    -> loc は NodeHeader へ (NodeData には入らない)
#   make<T>(loc, a, b, ...)         -> NodeData<T> を集成初期化
# 抽象ノードは get_arena<T>() の特殊化が無いのでコンパイルエラーになる (意図どおり)。
arena.write(
    "    template<class T, class... A>\n"
    "    constexpr Ref<Arena,T> make(lexer::Loc loc = {}, A&&... args) {\n"
    "        auto& pool = get_arena<T>();\n"
    "        pool.push_back(NodeData<T>{std::forward<A>(args)...});\n"
    "        headers.push_back(NodeHeader{.type = get_node_type<T>(),\n"
    "                                     .data_index = static_cast<std::uint32_t>(pool.size() - 1),\n"
    "                                     .loc = loc});\n"
    "        return Ref<Arena,T>{this,Node<T>{static_cast<std::uint32_t>(headers.size()),get_node_type<T>()}};\n"
    "    }\n"
)
arena.write("    template<class T> constexpr NodeData<T>* get(Node<T> id);\n")
arena.write("    template<class T> constexpr bool is_valid(Node<T> id) const {\n")
arena.write("        if (id.id() == 0 || headers.size() < id.id()) { return false; }\n")
arena.write("        if (headers[id.id()  - 1].type != id.type()) { return false; }\n")
arena.write("        return true;\n")
arena.write("    }\n")
arena.write("    template<class T> constexpr Ref<Arena,T> as_ref(Node<T> id)  {\n")
arena.write("        if (!is_valid(id)) {  return {}; }\n")
arena.write("        return Ref<Arena,T>{this,id};\n")
arena.write("    }\n")
arena.write("    template<class T> constexpr NodeHeader* get_header(Node<T> id) {\n")
arena.write("        if (!is_valid(id)) { return nullptr; }\n")
arena.write("        return &headers[id.id() - 1];\n")
arena.write("    }\n")
arena.write("   private:\n")
arena.write("    std::vector<NodeHeader> headers;\n")
arena.write("   public:\n")

arena_as_json = Writer()
arena_as_json.write("    constexpr void as_json(auto&& s) const {\n")
arena_as_json.write("        auto obj_ = s.object();\n")
arena_as_json.write('        obj_("headers",headers);\n')


def switch_cases(node: dict):
    if not node.get("abstract", False):
        arena.write(
            "        case NodeType::",
            node["name"],
            ": return &data_",
            node["name"],
            "[headers[id.id() - 1].data_index];\n",
        )
    if derives.get(node["name"]) is not None:
        for l in derives[node["name"]]["list"]:
            switch_cases([n for n in nodes if n["name"] == l][0])


for node in nodes:
    abstract = node.get("abstract", False)
    arena.write(
        "    template<> NodeData<",
        node["name"],
        ">* get(Node<",
        node["name"],
        "> id) {\n",
    )
    arena.write("        if (!is_valid(id)) { return nullptr; }\n")
    arena.write("        switch (id.type()) {\n")
    switch_cases(node)
    arena.write("        default: return nullptr;\n")
    arena.write("        }\n")
    arena.write("    }\n")
    if abstract == True:
        continue
    arena.write("   private:\n")
    arena.write(
        "    std::vector<NodeData<", node["name"], ">> data_", node["name"], ";\n"
    )
    arena_as_json.write(
        '        obj_("data_', node["name"], '",data_', node["name"], ");\n"
    )
    arena.write("   public:\n")
    arena.write("    template<>\n")
    arena.write(
        "    constexpr auto& get_arena<ordinal(NodeType::",
        node["name"],
        ")>() { return data_",
        node["name"],
        "; }\n",
    )


arena_as_json.write("    }")
arena.write(arena_as_json.getvalue())


arena.write("};\n")


# ---- ノードではない素の値型 (CommentRange など) --------------------------------
# side table の値や NodeData のフィールドから使う。NodeType にも arena にも入らない。
struct_def = Writer()
for struct in structs:
    struct_def.write("struct ", struct["name"], " {\n")
    print_node_field(struct_def, struct)
    struct_def.write("};\n")


# ---- side table ---------------------------------------------------------------
# 解析結果をノードの外に置くための表。キーは Node::id() (headers の添字) で統一し、
# 密度に応じて storage を選ぶ。API は storage によらず contains/get/set で共通なので、
# 密度の見積もりを外しても宣言を 1 語変えて再生成すれば済む。
table_def = Writer()


def emit_side_table(t: dict):
    name = t["name"]
    over = t["over"]
    storage = t.get("storage", "sparse")
    if storage not in ("dense", "sparse", "flag"):
        raise ValueError(f"unknown storage for side table {name}: {storage}")
    if not any(n["name"] == over for n in nodes):
        raise ValueError(f"unknown node type in side table {name}: {over}")
    table = name + "Table"
    node_t = f"Node<{over}>"

    # flag は値を持たないが、table<T>() のキーとして使うためタグ型は出す。
    table_def.write("struct ", name, " {\n")
    print_node_field(table_def, t if storage != "flag" else {"fields": []})
    table_def.write("};\n")

    table_def.write(f"// side table over {over} (storage: {storage})\n")
    table_def.write(f"struct {table} {{\n")
    table_def.write(f"    using node_type = {over};\n")
    table_def.write(f"    using key_type = {name};\n")
    if storage != "flag":
        table_def.write(f"    using value_type = {name};\n")

    if storage == "dense":
        table_def.write(
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
    elif storage == "sparse":
        table_def.write(
            "   public:\n"
            "    struct Entry {\n"
            "        std::uint32_t node{};\n"
            "        value_type value{};\n"
            "        constexpr void as_json(auto&& s) const {\n"
            "            auto obj_ = s.object();\n"
            "            obj_(\"node\",node);\n"
            "            obj_(\"value\",value);\n"
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
    else:  # flag
        table_def.write(
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

    table_def.write("};\n")


for side_table in side_tables:
    emit_side_table(side_table)


# 宣言された enum と NodeType にヘルパ一式を出す。
# NodeType にも出すのは、全ノード種を走査したい場面 (ツール側) が実際にあるため。
for enum in enums:
    emit_enum_helpers(
        enum["name"],
        [(e["name"], e["name"], e.get("alt_names", [])) for e in enum["enums"]],
    )
emit_enum_helpers("NodeType", [(n["name"], n["name"], []) for n in nodes])


# 全 side table を集める集約。
# Arena に持たせると「持ち回す入れ物に構文と解析結果が同居する」形になり、
# side table を分けた意味が 1 段上で失われるので、構文木 (Arena) とは別の入れ物にする。
if side_tables:
    table_def.write("struct SideTables {\n")
    table_def.write("    template<class T> constexpr auto& table();\n")
    table_def.write("    template<class T> constexpr const auto& table() const;\n")
    for t in side_tables:
        name = t["name"]
        table_def.write("   private:\n")
        table_def.write(f"    {name}Table {name}_;\n")
        table_def.write("   public:\n")
        table_def.write(f"    template<> constexpr auto& table<{name}>() {{ return {name}_; }}\n")
        table_def.write(
            f"    template<> constexpr const auto& table<{name}>() const {{ return {name}_; }}\n"
        )
    table_def.write("    void as_json(auto&& s) const {\n")
    table_def.write("        auto obj_ = s.object();\n")
    for t in side_tables:
        name = t["name"]
        table_def.write(f'        obj_("{name}",{name}_);\n')
    table_def.write("    }\n")
    table_def.write("};\n")


with open(NODES_H, "w", encoding="utf-8") as w:
    # ast_enum.h / deep_copy.h と同じ体裁 (license -> 生成物である旨 -> pragma once)
    w.write("/*license*/\n")
    w.write('// Code generated by "nodegen.py"; DO NOT EDIT.\n')
    w.write("// edit nodes.json instead.\n")
    w.write("#pragma once\n")
    w.write(incl_def.getvalue())
    w.write("namespace brgen::nast {\n")
    w.write(fwd_def.getvalue())
    w.write(enum_def.getvalue())
    w.write(enum_helper_def.getvalue())
    w.write(constexpr_fn_def.getvalue())
    w.write(header_def.getvalue())
    w.write(struct_def.getvalue())
    w.write(body.getvalue())
    w.write(arena.getvalue())
    w.write(table_def.getvalue())
    w.write("}\n")
