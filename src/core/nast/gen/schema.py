"""nodes.json の読み込みと正規化。

生成の各段 (enums / nodes / arena / tables) はここが提供する問い合わせだけを使い、
JSON の生の形には触らない。フィールドの追加や構造の変更をここに閉じ込めるため。
"""

import json


class Schema:
    def __init__(self, doc: dict):
        self.doc = doc
        self.includes = doc["includes"]
        self.types = doc.get("types", {})
        self.enums = doc.get("enums", [])
        self.nodes = doc.get("nodes", [])
        self.header = doc["header"]
        # ノードではない素の値型と、解析結果を置く side table。どちらも無くてもよい。
        self.structs = doc.get("structs", [])
        self.side_tables = doc.get("side_tables", [])

        self._normalize_enum_layers()
        self._by_name = {n["name"]: n for n in self.nodes}
        self._derives = self._build_derives()

    @classmethod
    def load(cls, path: str) -> "Schema":
        with open(path, encoding="utf-8") as f:
            return cls(json.load(f))

    # ---- enum -----------------------------------------------------------------

    def _normalize_enum_layers(self) -> None:
        """layers を持つ enum は、層を平坦化したものが並びになる。

        ast::expr_layer.h では層の配列と enum の並びが別々に手書きされていて、
        ズレを check_layers() の static_assert で検出していた。ここでは層が単一の
        正本なのでズレ自体が起こらない。
        """
        for enum in self.enums:
            if "layers" in enum:
                enum["enums"] = [m for layer in enum["layers"] for m in layer["enums"]]

    @staticmethod
    def enum_members(enum: dict):
        """(cpp_name, name, alt_names) を宣言順で返す"""
        return [(e["name"], e["name"], e.get("alt_names", [])) for e in enum["enums"]]

    @staticmethod
    def primary_name(member: dict) -> str:
        """代表表記。alt_names があればその先頭、無ければ名前"""
        alts = member.get("alt_names", [])
        return alts[0] if alts else member["name"]

    # ---- ノードの継承 ----------------------------------------------------------

    def _build_derives(self) -> dict:
        """子を持つ型に 1 ビットずつ割り当てる。

        NodeType = (ordinal << bit_width) | derive_bits になっていて、
        is_derived<T>() はビット AND 1 回で済む。手書きの 0x010000 のような
        定数を振り直す必要がない。
        """
        derives: dict[str, dict] = {}
        for node in self.nodes:
            parent = node.get("derive")
            if parent is None:
                continue
            entry = derives.setdefault(parent, {"bit": 0, "list": []})
            entry["list"].append(node["name"])
        for i, name in enumerate(derives):
            derives[name]["bit"] = 1 << i
        return derives

    @property
    def derive_bit_width(self) -> int:
        return len(self._derives)

    def has_children(self, name: str) -> bool:
        return name in self._derives

    def own_bit(self, name: str) -> int:
        return self._derives[name]["bit"] if name in self._derives else 0

    def children_of(self, name: str):
        return [self._by_name[c] for c in self._derives.get(name, {}).get("list", [])]

    def derive_bits(self, node: dict) -> int:
        """自分のビット + 祖先すべてのビット"""
        bits = self.own_bit(node["name"])
        parent = node.get("derive")
        if parent is not None:
            bits |= self.own_bit(parent)
            bits |= self.derive_bits(self._by_name[parent])
        return bits

    def node_by_name(self, name: str) -> dict:
        return self._by_name[name]

    def concrete_nodes(self):
        return [n for n in self.nodes if not n.get("abstract")]

    def descendants_with_self(self, node: dict):
        """自分 (具象なら) と全子孫の具象ノードを、宣言順で"""
        out = []
        if not node.get("abstract"):
            out.append(node)
        for child in self.children_of(node["name"]):
            out += self.descendants_with_self(child)
        return out

    def all_fields(self, node: dict):
        """基底から順に並べたフィールド。derive を持たない値型でも使える。"""
        out = []
        parent = node.get("derive")
        if parent is not None:
            out += self.all_fields(self._by_name[parent])
        return out + list(node["fields"])

    # ---- 型 -------------------------------------------------------------------

    @staticmethod
    def resolve_type(t: str) -> str:
        return (
            t.replace("vector<", "std::vector<")
            .replace("string", "std::string")
            .replace("uint32", "std::uint32_t")
            .replace("size", "std::size_t")
        )

    def field_init(self, field: dict) -> str:
        resolved = self.resolve_type(field["type"])
        return field.get("init", self.types.get(resolved, {}).get("init", "{}"))
