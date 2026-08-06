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
    def write(self,*args):
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

[incl_def.write("#include ",x,"\n") for x in includes]

for enum in enums:
    max_alt_names = max([len(x.get("alt_names",[])) for x in enum["enums"]])
    enum_def.write("enum class ",enum["name"],"{\n")
    for elem in enum["enums"]:
        enum_def.write("    ",elem["name"]," = "+elem["value"] if "value" in elem else "",",\n")
    enum_def.write("};\n")
    enum_def.write("constexpr const char* to_string(",enum["name"]," t", ", size_t alt = 0" if max_alt_names > 0 else "" ,") {\n")
    enum_def.write("    switch (t) {\n")
    for elem in enum["enums"]:
        enum_def.write("    case ",enum["name"],"::",elem["name"],":\n")
        alt_names = elem.get("alt_names",[])
        if len(alt_names) > 0:
            enum_def.write("        switch (alt) {\n")
            for i,alt in enumerate(alt_names):
                enum_def.write("        case ",str(i+1),": return ",json.dumps(alt),";\n")
            enum_def.write("        }\n")
        enum_def.write("        return \"",elem["name"],"\";\n")
    enum_def.write("    }\n")
    enum_def.write("    return nullptr;\n")
    enum_def.write("}\n")
    enum_def.write("constexpr void as_json(",enum["name"]," t,auto&& s) {\n")
    enum_def.write("    s.string(to_string(t));\n")
    enum_def.write("}\n")


def resolve_type_name(t :str):
    return t.replace("vector<","std::vector<").replace("string","std::string").replace("uint32","std::uint32_t")

def print_field(body:Writer, node :dict):
    fields = node["fields"]
    if node.get("derive") is not None:
        print_field(body,[n for n in nodes if n["name"] == node["derive"]][0])
    for field in fields:
        body.write("        field(",json.dumps(field["name"]),",",field["name"],");\n")


def print_node_field(body :Writer, node :dict):
    fields = node["fields"]
    for field in fields:
        field_name = field["name"]
        field_type = resolve_type_name(field["type"])
        init = field.get("init",types.get(field_type,{}).get("init",""))
        body.write("    ",field_type," ",field_name,init,";\n")
    body.write("    constexpr void as_json(auto&& s) const {\n")
    body.write("        auto field = s.object();\n")
    print_field(body,node)
    body.write("    }\n")


header_def.write("struct NodeHeader {\n")
print_node_field(header_def,node_def["header"])
header_def.write("};\n")
header_def.write("template <class T>\n")
header_def.write("struct Node {\n")
header_def.write("    friend struct Arena;\n")
header_def.write("    template<class> friend struct Node;")
header_def.write("    constexpr Node() : id_{0}, type_{get_node_type<T>()} {}\n")
header_def.write("   private:\n")
header_def.write("    std::uint32_t id_{};\n")
header_def.write("    NodeType type_{};\n")
header_def.write("    constexpr Node(std::uint32_t id,NodeType type) : id_{id},type_{type} {}\n")
header_def.write("   public:\n")
header_def.write("    template<std::derived_from<T> U>  constexpr Node(Node<U> x) : id_{x.id_},type_{x.type_} {}\n")
header_def.write("    constexpr NodeType type() const { return type_; }\n")
header_def.write("    constexpr std::uint32_t id() const { return id_; }\n")
header_def.write("    constexpr bool is_null() const { return id_ == 0; }")
header_def.write("    constexpr explicit operator bool() const { return !is_null(); }")
header_def.write("    constexpr std::uint64_t unique_id() const { return (std::uint64_t(type_) << 32) | id_; }\n")
header_def.write("    constexpr void as_json(auto&& s) const { s.number(unique_id()); }\n")
header_def.write("    template<class A> constexpr auto ref(A& a) { return a.as_ref(*this); }\n")
# U は T の派生なので、これは基底 -> 派生 のチェック付きダウンキャスト。
# 既存 AST の ast::as<T>(node) と同じ意味なので名前を合わせる。
header_def.write("    template<class U> requires std::derived_from<U,T> constexpr Node<U> as() const { return is_derived<U>(type_) ? Node<U>{id_,type_} : Node<U>{};  }\n")
header_def.write("};\n")
header_def.write("template <class T>\n")
header_def.write("struct NodeData;\n")
enum_def.write("enum class NodeType : std::uint32_t {\n")

derives = {}

for node in nodes:
    if "derive" in node:
        l = derives.get(node["derive"],{"bit":0, "list": []})
        l["list"].append(node["name"])
        derives[node["derive"]] = l  

i = 0
for derive in derives:
    derives[derive]["bit"] = 1 << i 
    i+=1 
print(derives)

def get_derive_bit(node :dict):
    bit = 0
    if node["name"] in derives:
        bit |= derives[node["name"]]["bit"]
    if "derive" in node:
        bit |= derives[node["derive"]]["bit"]
        bit |= get_derive_bit([n for n in nodes if n["name"] == node["derive"]][0])
    return bit

for i,node in enumerate(nodes):
    name = node["name"]
    enum_def.write("    ",name," = (",str(i)," << ",str(len(derives)),") | ",str(get_derive_bit(node)),",\n")
    constexpr_fn_def.write("template<> constexpr NodeType get_node_type<",name,">() { return NodeType::",name,"; }\n")
    fwd_def.write("struct ",name,":" + node["derive"] if "derive" in node else "" ,"{};\n")
    body.write("template<>\n")
    body.write("struct NodeData<",name,">", (": NodeData<"+node["derive"] + ">" if "derive" in node else "") , " {\n")
    print_node_field(body,node)
    body.write("};\n")

enum_def.write("};\n")
enum_def.write("template<class T> constexpr NodeType get_node_type();\n")
enum_def.write("template<class T> constexpr bool is_derived(NodeType);\n")
enum_def.write("constexpr std::uint32_t ordinal(NodeType t) { return std::uint32_t(t) >> ",str(len(derives)),"; }\n")
enum_def.write("constexpr const char* to_string(NodeType t) {\n")
enum_def.write("    switch(t) {")
for i,node in enumerate(nodes):
    name = node["name"]
    enum_def.write("    case NodeType::",name,": return \"",name,"\";\n")
enum_def.write("    default: return nullptr;")
enum_def.write("    }\n")
enum_def.write("}\n")
enum_def.write("constexpr void as_json(NodeType t,auto&& s) { s.string(to_string(t)); }")


for node in nodes:
    if node["name"] in derives:
        enum_def.write("template<> constexpr bool is_derived<",node["name"],">(NodeType t) { return std::uint32_t(t) & ",str(derives[node["name"]]["bit"]),"; }\n")
    else:
        enum_def.write("template<> constexpr bool is_derived<",node["name"],">(NodeType t) { return t == NodeType::",node["name"],"; }\n")


enum_def.write("struct Arena;\n")
header_def.write("template<class A,class T>\n")
header_def.write("struct Ref {\n")
header_def.write("    friend struct Arena;\n")
header_def.write("    constexpr Ref() = default;\n")
header_def.write("   private:\n")
header_def.write("    A* arena_ = nullptr;\n")
header_def.write("    Node<T> id_;\n")
header_def.write("    constexpr Ref(A* a,Node<T> id): arena_{a},id_{id} {}\n")
header_def.write("   public:\n")
header_def.write("    constexpr Node<T> id() const {  return id_; }\n")
header_def.write("    constexpr A* arena() { return arena_; }\n")
# Node<T> だけでなく基底の Node<U> へも直接変換できるようにする。
# operator Node<T> のみだと Ref -> Node<T> -> Node<U> がユーザー定義変換 2 段になり、
# vector<Node<Statement>>::push_back(Ref<Arena,Field>) のような呼び出しが通らない。
header_def.write("    template<class U> requires std::derived_from<T,U>\n")
header_def.write("    constexpr operator Node<U>() const  { return id_; }\n")
header_def.write("    constexpr NodeData<T>* get() { return arena_ ? arena_->template get<T>(id_) : nullptr; }\n")
header_def.write("    constexpr NodeData<T>* operator->() { return get(); }\n")
header_def.write("};\n")

arena.write("struct Arena {\n")
arena.write("    template<std::uint32_t ord> constexpr auto& get_arena();\n")
arena.write("    template<class T> constexpr auto& get_arena() {  return get_arena<ordinal(get_node_type<T>())>(); }\n")
arena.write("    template<class T> constexpr Ref<Arena,T> make();\n")
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
arena.write("   private:\n")
arena.write("    std::vector<NodeHeader> headers;\n")
arena.write("   public:\n")

arena_as_json = Writer()
arena_as_json.write("    constexpr void as_json(auto&& s) const {\n")
arena_as_json.write("        auto field = s.object();\n")
arena_as_json.write("        field(\"headers\",headers);\n")




def switch_cases(node :dict):
    if not node.get("abstract",False):
        arena.write("        case NodeType::",node["name"],": return &data_",node["name"],"[headers[id.id() - 1].data_index];\n")
    if derives.get(node["name"]) is not None:
        for l in derives[node["name"]]["list"]:
            switch_cases([n for n in nodes if n["name"] == l][0])

for node in nodes:
    abstract = node.get("abstract",False) 
    arena.write("    template<> NodeData<",node["name"],">* get(Node<",node["name"],"> id) {\n")
    arena.write("        if (!is_valid(id)) { return nullptr; }\n")
    arena.write("        switch (id.type()) {\n")
    switch_cases(node)
    arena.write("        default: return nullptr;\n")
    arena.write("        }\n")
    arena.write("    }\n")
    if abstract == True:
        continue
    arena.write("   private:\n")
    arena.write("    std::vector<NodeData<",node["name"],">> data_",node["name"],";\n")
    arena_as_json.write("        field(\"data_",node["name"],"\",data_",node["name"],");\n")
    arena.write("   public:\n")
    arena.write("    template<>\n")
    arena.write("    constexpr auto& get_arena<ordinal(NodeType::",node["name"],")>() { return data_",node["name"],"; }\n")
    arena.write("    template<> Ref<Arena,",node["name"],"> make() {\n")
    arena.write("        get_arena<",node["name"],">().push_back({});\n")
    arena.write("        headers.push_back(NodeHeader{.type = get_node_type<",node["name"],">(),.data_index = static_cast<std::uint32_t>(get_arena<",node["name"],">().size() - 1)});\n")
    arena.write("        Node<",node["name"],"> id{static_cast<std::uint32_t>(headers.size()),get_node_type<",node["name"],">()};\n")
    arena.write("        return Ref<Arena,",node["name"],">{this,id};\n")
    arena.write("    }\n")
  


arena_as_json.write("    }")
arena.write(arena_as_json.getvalue())



arena.write("};\n")

with open(NODES_H, "w", encoding="utf-8") as w:
    w.write(incl_def.getvalue())
    w.write("namespace brgen::nast {\n")
    w.write(fwd_def.getvalue())
    w.write(enum_def.getvalue())
    w.write(constexpr_fn_def.getvalue())
    w.write(header_def.getvalue())
    w.write(body.getvalue())
    w.write(arena.getvalue())
    w.write("}\n")

