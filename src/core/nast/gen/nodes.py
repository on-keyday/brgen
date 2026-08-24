"""NodeType / NodeHeader / Node / RefBase / NodeData と、型を静的に持たない走査の入口。"""

import json

from .schema import Schema
from .writer import Writer


def emit_fields(w: Writer, schema: Schema, node: dict, self_type: str = "") -> None:
    """フィールド宣言 + as_json + for_each_field。

    NodeData<T> だけでなく NodeHeader や素の値型でも同じものを出す。
    走査 (pretty printer / from_json) がこの 2 つだけを前提にできるようにするため。
    """
    for field in node["fields"]:
        w.write(
            "    ",
            schema.resolve_type(field["type"]),
            " ",
            field["name"],
            schema.field_init(field),
            ";\n",
        )

    w.write("    constexpr void as_json(auto&& s) const {\n")
    w.write("        auto obj_ = s.object();\n")
    for field in schema.all_fields(node):
        # ローカル名は obj_ にしてある。field という名前のフィールドを持つノード
        # (UnionCandidate::field) があり、field だとローカルが実体を隠すため。
        w.write("        obj_(", json.dumps(field["name"]), ",", field["name"], ");\n")
    w.write("    }\n")

    for const in ("", " const"):
        w.write("    constexpr void for_each_field(auto&& f_)", const, " {\n")
        for field in schema.all_fields(node):
            # weak は所有しない逆向き参照。木として辿るときに降りてはいけない辺。
            weak = "true" if field.get("weak") else "false"
            w.write(
                "        f_(",
                json.dumps(field["name"]),
                ",",
                field["name"],
                ",",
                weak,
                ");\n",
            )
        w.write("    }\n")

    # 2 つを同じ順で並べて回す版。比較 (compare.h) が要る。
    # cosmetic は「論理的等価性に効かない」印で、weak と同じく schema 側の宣言。
    if not self_type:
        return
    w.write("    constexpr void for_each_field(const ", self_type, "& o_, auto&& f_) const {\n")
    for field in schema.all_fields(node):
        weak = "true" if field.get("weak") else "false"
        cosmetic = "true" if field.get("cosmetic") else "false"
        w.write(
            "        f_(",
            json.dumps(field["name"]),
            ",",
            field["name"],
            ",o_.",
            field["name"],
            ",",
            weak,
            ",",
            cosmetic,
            ");\n",
        )
    w.write("    }\n")


def emit_node_header(w: Writer, schema: Schema) -> None:
    w.write("struct NodeHeader {\n")
    emit_fields(w, schema, schema.header, "NodeHeader")
    w.write("};\n")


def emit_node(w: Writer) -> None:
    # T = void は「どのノードでもよい」。T は phantom で、実体は id と NodeType の
    # 8 バイトなので消しても情報は落ちない。何が来るか実行時にしか決まらない側
    # (走査など) が、ノード種の数だけテンプレートを増やさずに済む。
    # 別名は NodeAny。具体型に戻すのは as_any<U>()。
    w.write("template <class T>\n")
    w.write("struct Node {\n")
    w.write("    friend struct Arena;\n")
    w.write("    template<class> friend struct Node;")
    w.write("    static constexpr NodeType own_type() {\n")
    w.write("        if constexpr (std::is_void_v<T>) { return NodeType{}; }\n")
    w.write("        else { return get_node_type<T>(); }\n")
    w.write("    }\n")
    w.write("    constexpr Node() : id_{0}, type_{own_type()} {}\n")
    w.write("   private:\n")
    w.write("    std::uint32_t id_{};\n")
    w.write("    NodeType type_{};\n")
    w.write("    constexpr Node(std::uint32_t id,NodeType type) : id_{id},type_{type} {}\n")
    w.write("   public:\n")
    # T が void ならどの Node からも受ける。そうでなければ従来どおり派生からのみ。
    w.write("    template<class U> requires (std::is_void_v<T> || std::derived_from<U,T>)\n")
    w.write("    constexpr Node(Node<U> x) : id_{x.id_},type_{x.type_} {}\n")
    w.write("    constexpr Node(nullref_t) : Node() {}\n")
    # 比較は id_ のみ。= default だと type_ も比べてしまい、既定構築した
    # Node<Format>{} と Node<Function>{} が (どちらも null なのに) 不一致になる。
    # friend なので派生 -> 基底の暗黙変換と nullref_t からの変換が両辺に効き、
    # Node<Format> == Node<Function> のような無関係な比較はコンパイルエラーになる。
    w.write(
        "    friend constexpr bool operator==(const Node& l, const Node& r) { return l.id_ == r.id_; }\n"
        "    friend constexpr auto operator<=>(const Node& l, const Node& r) { return l.id_ <=> r.id_; }\n"
    )
    w.write("    constexpr NodeType type() const { return type_; }\n")
    w.write("    constexpr std::uint32_t id() const { return id_; }\n")
    w.write("    constexpr bool is_null() const { return id_ == 0; }")
    w.write("    constexpr explicit operator bool() const { return !is_null(); }")
    w.write("    constexpr std::uint64_t unique_id() const { return (std::uint64_t(type_) << 32) | id_; }\n")
    w.write("    constexpr void as_json(auto&& s) const { s.number(unique_id()); }\n")
    # unique_id() と対称にしておく。from_json はここから復元する。
    w.write(
        "    static constexpr Node from_unique_id(std::uint64_t v) {\n"
        "        return Node{static_cast<std::uint32_t>(v & 0xffffffffu), NodeType(static_cast<std::uint32_t>(v >> 32))};\n"
        "    }\n"
    )
    w.write("    template<class A> constexpr auto ref(A& a) const { return a.as_ref(*this); }\n")
    # 名前でフィールドを辿る。走査の実体は access.h にあるので、
    # nodes.h だけを include した状態では実体化できない。
    w.write("    template<fixed_string Path, class A> constexpr auto field(A& a) const"
            " { return node_field<Path>(a, *this); }\n")
    # U は T の派生なので基底 -> 派生 のチェック付きダウンキャスト。
    # 既存 AST の ast::as<T>(node) と同じ意味なので名前を合わせる。
    w.write(
        "    template<class U> requires std::derived_from<U,T> constexpr Node<U> as() const"
        " { return as_any<U>();  }\n"
    )
    # 派生関係の制約を外した版。generic なコードでは T が U の基底とは限らないので
    # as<U>() が通らない。判定自体は type_ のビットを見るだけなので静的な関係は要らず、
    # 当たらなければ null が返る。
    w.write(
        "    template<class U> constexpr Node<U> as_any() const"
        " { return is_derived<U>(type_) ? Node<U>{id_,type_} : Node<U>{};  }\n"
    )
    w.write("};\n")
    w.write("// 種類を問わないノード参照。どの Node<T> からも暗黙に作れる。\n")
    w.write("using NodeAny = Node<void>;\n")
    w.write("template <class T>\n")
    w.write("struct NodeData;\n")


def emit_node_type(w: Writer, schema: Schema) -> None:
    """NodeType 本体と、種別の問い合わせ (ordinal / to_string / is_derived)。"""
    width = schema.derive_bit_width
    w.write("enum class NodeType : std::uint32_t {\n")
    for i, node in enumerate(schema.nodes):
        w.write(
            "    ", node["name"], " = (", str(i), " << ", str(width), ") | ",
            str(schema.derive_bits(node)), ",\n",
        )
    w.write("};\n")
    w.write("template<class T> constexpr NodeType get_node_type();\n")
    w.write("template<class T> constexpr bool is_derived(NodeType);\n")
    w.write("constexpr std::uint32_t ordinal(NodeType t) { return std::uint32_t(t) >> ", str(width), "; }\n")
    w.write("constexpr const char* to_string(NodeType t) {\n")
    w.write("    switch(t) {")
    for node in schema.nodes:
        w.write("    case NodeType::", node["name"], ': return "', node["name"], '";\n')
    w.write("    default: return nullptr;")
    w.write("    }\n")
    w.write("}\n")
    w.write("constexpr void as_json(NodeType t,auto&& s) { s.string(to_string(t)); }")

    for node in schema.nodes:
        name = node["name"]
        if schema.has_children(name):
            w.write(
                "template<> constexpr bool is_derived<", name,
                ">(NodeType t) { return std::uint32_t(t) & ", str(schema.own_bit(name)), "; }\n",
            )
        else:
            w.write(
                "template<> constexpr bool is_derived<", name,
                ">(NodeType t) { return t == NodeType::", name, "; }\n",
            )

    w.write("struct Arena;\n")
    w.write("struct nullref_t {};\n")
    w.write("constexpr nullref_t nullref{};\n")


def emit_forward_decls(w: Writer, schema: Schema) -> None:
    """タグ型。継承関係だけを持ち、実データは NodeData<T> にある。"""
    for node in schema.nodes:
        w.write("struct ", node["name"], ":" + node["derive"] if "derive" in node else "", "{};\n")


def emit_get_node_type(w: Writer, schema: Schema) -> None:
    for node in schema.nodes:
        w.write(
            "template<> constexpr NodeType get_node_type<", node["name"],
            ">() { return NodeType::", node["name"], "; }\n",
        )


def emit_ref(w: Writer) -> None:
    """構築時の足場。Node<T> は id だけなので、arena を持つこちらが -> を提供する。"""
    w.write("template<class A,class T>\n")
    w.write("struct RefBase {\n")
    w.write("    friend struct Arena;\n")
    w.write("    template<class,class> friend struct RefBase;\n")
    w.write("    constexpr RefBase() = default;\n")
    w.write("   private:\n")
    w.write("    A* arena_ = nullptr;\n")
    w.write("    Node<T> id_;\n")
    w.write("    constexpr RefBase(A* a,Node<T> id): arena_{a},id_{id} {}\n")
    w.write("   public:\n")
    w.write("    constexpr RefBase(nullref_t) : arena_{nullptr},id_{} {}\n")
    w.write("    constexpr Node<T> id() const {  return id_; }\n")
    w.write("    constexpr A* arena() { return arena_; }\n")
    # arena を持っているので引数を取らない。
    w.write("    template<fixed_string Path> constexpr auto field() const"
            " { return node_field<Path>(*arena_, id_); }\n")
    w.write("    template<class U> requires std::derived_from<U,T>\n")
    w.write("    constexpr RefBase<A,U> as() const { return as_any<U>(); }\n")
    # arena_ はポインタなので ref(A&) には渡せない。実体化されるまで気づかなかった。
    w.write("    template<class U> constexpr RefBase<A,U> as_any() const {\n")
    w.write("        auto n = id_.template as_any<U>();\n")
    w.write("        return n ? RefBase<A,U>{arena_,n} : RefBase<A,U>{};\n")
    w.write("    }\n")
    w.write("    template<class U> requires std::derived_from<T,U>\n")
    w.write("    constexpr operator RefBase<A,U>() const { return RefBase<A,U>{arena_,id_}; }\n")
    # Node<T> だけでなく基底の Node<U> へも直接変換できるようにする。
    # operator Node<T> のみだと RefBase -> Node<T> -> Node<U> がユーザー定義変換 2 段になり、
    # vector<Node<Statement>>::push_back(RefBase<Arena,Field>) が通らない。
    w.write("    template<class U> requires std::derived_from<T,U>\n")
    w.write("    constexpr operator Node<U>() const  { return id_; }\n")
    w.write("    constexpr NodeData<T>* get() { return arena_ ? arena_->get(id_) : nullptr; }\n")
    w.write("    constexpr const NodeData<T>* get() const { return arena_ ? arena_->get(id_) : nullptr; }\n")
    w.write("    constexpr NodeData<T>* operator->() { return get(); }\n")
    w.write("    constexpr const NodeData<T>* operator->() const { return get(); }\n")
    # loc は NodeData ではなく NodeHeader にあるので payload 経由では届かない。
    # 旧 AST の node->loc に相当する経路をここで用意する。
    w.write(
        "    constexpr lexer::Loc loc() const {\n"
        "        auto* h = arena_ ? arena_->get_header(id_) : nullptr;\n"
        "        return h ? h->loc : lexer::Loc{};\n"
        "    }\n"
        "    constexpr void set_loc(lexer::Loc l) {\n"
        "        if (auto* h = arena_ ? arena_->get_header(id_) : nullptr) { h->loc = l; }\n"
        "    }\n"
    )
    w.write("    constexpr NodeData<T>& operator*() { return *get(); }\n")
    w.write("    constexpr const NodeData<T>& operator*() const { return *get(); }\n")
    w.write("    constexpr explicit operator bool() const { return arena_ && !id_.is_null(); }\n")
    w.write("    constexpr NodeHeader* header() { return arena_ ? arena_->get_header(id_) : nullptr; }\n")
    w.write("};\n")


def emit_node_data(w: Writer, schema: Schema) -> None:
    """payload。基底の NodeData を継承するので、抽象型へのアップキャストが効く。"""
    for node in schema.nodes:
        w.write("template<>\n")
        w.write(
            "struct NodeData<", node["name"], ">",
            (": NodeData<" + node["derive"] + ">" if "derive" in node else ""), " {\n",
        )
        emit_fields(w, schema, node, "NodeData<" + node["name"] + ">")
        w.write("};\n")


def emit_dispatch(w: Writer, schema: Schema) -> None:
    """NodeType から具象型へ降りる。抽象 Node しか無い場所で走査するのに要る。"""
    w.write("template<class T> struct node_tag { using type = T; };\n")
    w.write("constexpr decltype(auto) visit_node_type(NodeType t, auto&& f) {\n")
    w.write("    switch (t) {\n")
    for node in schema.concrete_nodes():
        w.write(f"    case NodeType::{node['name']}: return f(node_tag<{node['name']}>{{}});\n")
    w.write("    default: break;\n")
    w.write("    }\n")
    w.write(f"    return f(node_tag<{schema.concrete_nodes()[0]['name']}>{{}});\n")
    w.write("}\n")
