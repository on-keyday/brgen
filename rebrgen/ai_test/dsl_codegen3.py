#!/usr/bin/env python3
import re
import sys
import struct


# --- AST ノードの定義 ---
class Node:
    pass


class Field(Node):
    def __init__(self, type_name, name, length=None):
        self.type_name = type_name  # 例: "uint8", "byte" など
        self.name = name  # フィールド名
        self.length = length  # 配列の場合、リテラルまたはフィールド参照


class Conditional(Node):
    def __init__(self):
        # branches は (condition, nodes) のタプルのリスト
        # condition が None の場合は else ブランチ
        self.branches = []


class Format:
    def __init__(self, name, nodes):
        self.name = name  # フォーマット名
        self.nodes = nodes  # Field や Conditional のリスト


# --- DSL パーサー ---
def parse_dsl(dsl_text):
    # "format <Name> { ... }" の中身を抽出
    m = re.search(r"format\s+(\w+)\s*{(.*)}\s*$", dsl_text, re.DOTALL)
    if not m:
        raise ValueError("DSL の形式が不正です。")
    fmt_name = m.group(1)
    body_text = m.group(2).strip()
    nodes, _ = parse_nodes(body_text, 0)
    return Format(fmt_name, nodes)


def skip_whitespace(text, i):
    while i < len(text) and text[i].isspace():
        i += 1
    return i


def parse_nodes(text, i):
    nodes = []
    while i < len(text):
        i = skip_whitespace(text, i)
        if i >= len(text):
            break
        if text.startswith("if", i):
            cond_node, i = parse_conditional(text, i)
            nodes.append(cond_node)
        elif text.startswith("else", i):
            # else は条件分岐の内部で処理すべきため、ここで出現したらブロック終了とみなす
            break
        elif text[i] == "}":  # 現在のブロックの終了
            break
        else:
            # 修正前: m = re.compile(r'(\w+)\s+(\w+)(?:\[(\w+)\])?\s*;').match(text, i)
            m = re.compile(r"(\w+)(?:\[(\w+)\])?\s+(\w+)\s*;").match(text, i)
            if not m:
                raise ValueError("フィールド定義のパースに失敗: " + text[i:])
            type_name = m.group(1)
            length = m.group(2)  # 存在しなければ None
            name = m.group(3)
            nodes.append(Field(type_name, name, length))
            i = m.end()
    return nodes, i


def parse_conditional(text, i):
    # if ブロックおよびそれに続く else if / else をまとめて 1 つの Conditional ノードとする
    cond_node = Conditional()
    while True:
        i = skip_whitespace(text, i)
        if text.startswith("if", i) or text.startswith("else if", i):
            # "if" または "else if" ブランチ
            if text.startswith("if", i):
                i += 2
            else:
                i += len("else if")
            i = skip_whitespace(text, i)
            # 条件部分を '{' まで抽出
            cond_end = text.find("{", i)
            if cond_end == -1:
                raise ValueError("条件ブロックの開始 '{' が見つかりません。")
            condition = text[i:cond_end].strip()
            # ブロック内部を解析
            i = cond_end + 1  # '{' をスキップ
            branch_nodes, i = parse_nodes(text, i)
            i = skip_whitespace(text, i)
            if i >= len(text) or text[i] != "}":
                raise ValueError("対応する '}' が見つかりません。")
            i += 1  # '}' をスキップ
            cond_node.branches.append((condition, branch_nodes))
        elif text.startswith("else", i):
            # else ブランチ（条件なし）
            i += len("else")
            i = skip_whitespace(text, i)
            if i >= len(text) or text[i] != "{":
                raise ValueError("else ブロックの開始 '{' が見つかりません。")
            i += 1  # '{' をスキップ
            branch_nodes, i = parse_nodes(text, i)
            i = skip_whitespace(text, i)
            if i >= len(text) or text[i] != "}":
                raise ValueError("対応する '}' が見つかりません。")
            i += 1  # '}' をスキップ
            cond_node.branches.append((None, branch_nodes))
            break  # else の後は続かない
        else:
            break
        i = skip_whitespace(text, i)
        # 次が else if/else でなければループ終了
        if not (text.startswith("else if", i) or text.startswith("else", i)):
            break
    return cond_node, i


# --- 型から struct 用フォーマット指定子への変換 ---
def get_struct_format(type_name):
    mapping = {
        "uint8": "B",
        "uint16": "H",
        "uint32": "I",
        "uint64": "Q",
        "float": "f",
        "double": "d",
        "byte": "c",  # byte は単一バイトの char として扱う
    }
    return mapping.get(type_name)


# --- 条件式の変換 ---
# シリアライズ用は self. を付与する（例: "header == 1" → "if self.header == 1:"）
def transform_condition_serialize(cond):
    def repl(match):
        word = match.group(0)
        # 数値や演算子、キーワードの場合はそのまま
        if word.isdigit() or word in {"and", "or", "not"} or word.startswith("self."):
            return word
        return "self." + word

    return re.sub(r"\b[a-zA-Z_]\w*\b", repl, cond)


# --- コード生成 ---
def generate_code(fmt: Format) -> str:
    lines = []
    lines.append("import struct")
    lines.append("")
    lines.append(f"class {fmt.name}:")
    # __init__ では、すべてのフィールド（条件分岐内も含む）を引数に取る簡易実装
    all_fields = collect_all_field_names(fmt.nodes)
    if all_fields:
        params = ", ".join(all_fields)
        lines.append(f"    def __init__(self, {params}):")
        for fname in all_fields:
            lines.append(f"        self.{fname} = {fname}")
    else:
        lines.append("    def __init__(self):")
        lines.append("        pass")
    lines.append("")
    # シリアライズ
    lines.append("    def serialize(self):")
    lines.append("        parts = []")
    generate_nodes_serialize(fmt.nodes, lines, indent="        ")
    lines.append("        return b''.join(parts)")
    lines.append("")
    # デシリアライズ
    lines.append("    @classmethod")
    lines.append("    def deserialize(cls, data):")
    lines.append("        offset = 0")
    generate_nodes_deserialize(fmt.nodes, lines, indent="        ")
    cons_params = ", ".join(all_fields)
    lines.append(f"        return cls({cons_params})")
    return "\n".join(lines)


def collect_all_field_names(nodes):
    names = []
    for node in nodes:
        if isinstance(node, Field):
            names.append(node.name)
        elif isinstance(node, Conditional):
            for _, branch_nodes in node.branches:
                names.extend(collect_all_field_names(branch_nodes))
    # 重複を除く（定義順を保持）
    seen = set()
    result = []
    for name in names:
        if name not in seen:
            seen.add(name)
            result.append(name)
    return result


def generate_nodes_serialize(nodes, lines, indent):
    for node in nodes:
        if isinstance(node, Field):
            if node.length is None:
                fmt_spec = get_struct_format(node.type_name)
                if fmt_spec:
                    lines.append(
                        f"{indent}parts.append(struct.pack('!{fmt_spec}', self.{node.name}))"
                    )
                else:
                    lines.append(f"{indent}parts.append(self.{node.name}.serialize())")
            else:
                # 配列の場合
                if node.length.isdigit():
                    fmt_spec = get_struct_format(node.type_name)
                    lines.append(f"{indent}for item in self.{node.name}:")
                    if fmt_spec:
                        lines.append(
                            f"{indent}    parts.append(struct.pack('!{fmt_spec}', item))"
                        )
                    else:
                        lines.append(f"{indent}    parts.append(item.serialize())")
                else:
                    fmt_spec = get_struct_format(node.type_name)
                    lines.append(f"{indent}for item in self.{node.name}:")
                    if fmt_spec:
                        lines.append(
                            f"{indent}    parts.append(struct.pack('!{fmt_spec}', item))"
                        )
                    else:
                        lines.append(f"{indent}    parts.append(item.serialize())")
        elif isinstance(node, Conditional):
            first = True
            for cond, branch_nodes in node.branches:
                if cond is not None:
                    cond_expr = transform_condition_serialize(cond)
                    if first:
                        lines.append(f"{indent}if {cond_expr}:")
                        first = False
                    else:
                        lines.append(f"{indent}elif {cond_expr}:")
                else:
                    lines.append(f"{indent}else:")
                generate_nodes_serialize(branch_nodes, lines, indent + "    ")


def generate_nodes_deserialize(nodes, lines, indent):
    for node in nodes:
        if isinstance(node, Field):
            if node.length is None:
                fmt_spec = get_struct_format(node.type_name)
                if fmt_spec:
                    size = struct.calcsize("!" + fmt_spec)
                    lines.append(
                        f"{indent}{node.name} = struct.unpack('!{fmt_spec}', data[offset:offset+{size}])[0]"
                    )
                    lines.append(f"{indent}offset += {size}")
                else:
                    lines.append(
                        f"{indent}# ネストされた型のデシリアライズが必要: {node.name}"
                    )
                    lines.append(
                        f"{indent}{node.name} = {node.type_name}.deserialize(data[offset:])"
                    )
                    lines.append(f"{indent}# offset 更新が必要")
            else:
                if node.length.isdigit():
                    fmt_spec = get_struct_format(node.type_name)
                    lines.append(f"{indent}{node.name} = []")
                    if fmt_spec:
                        size = struct.calcsize("!" + fmt_spec)
                        lines.append(f"{indent}for _ in range({node.length}):")
                        lines.append(
                            f"{indent}    item = struct.unpack('!{fmt_spec}', data[offset:offset+{size}])[0]"
                        )
                        lines.append(f"{indent}    {node.name}.append(item)")
                        lines.append(f"{indent}    offset += {size}")
                    else:
                        lines.append(f"{indent}for _ in range({node.length}):")
                        lines.append(
                            f"{indent}    item = {node.type_name}.deserialize(data[offset:])"
                        )
                        lines.append(f"{indent}    {node.name}.append(item)")
                        lines.append(f"{indent}    # offset 更新が必要")
                else:
                    fmt_spec = get_struct_format(node.type_name)
                    lines.append(f"{indent}{node.name} = []")
                    lines.append(
                        f"{indent}for _ in range({node.length}):  # {node.length} は既に読み出したフィールド"
                    )
                    if fmt_spec:
                        size = struct.calcsize("!" + fmt_spec)
                        lines.append(
                            f"{indent}    item = struct.unpack('!{fmt_spec}', data[offset:offset+{size}])[0]"
                        )
                        lines.append(f"{indent}    {node.name}.append(item)")
                        lines.append(f"{indent}    offset += {size}")
                    else:
                        lines.append(
                            f"{indent}    item = {node.type_name}.deserialize(data[offset:])"
                        )
                        lines.append(f"{indent}    {node.name}.append(item)")
                        lines.append(f"{indent}    # offset 更新が必要")
        elif isinstance(node, Conditional):
            # すでに読み出したフィールドを用いて条件分岐
            first = True
            for cond, branch_nodes in node.branches:
                if cond is not None:
                    # デシリアライズではローカル変数として評価（self. は不要）
                    if first:
                        lines.append(f"{indent}if {cond}:")
                        first = False
                    else:
                        lines.append(f"{indent}elif {cond}:")
                else:
                    lines.append(f"{indent}else:")
                generate_nodes_deserialize(branch_nodes, lines, indent + "    ")


# --- メイン処理 ---
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("使い方: python dsl_codegen.py <dsl_file>")
        sys.exit(1)
    dsl_file = sys.argv[1]
    with open(dsl_file, "r") as f:
        dsl_text = f.read()
    fmt = parse_dsl(dsl_text)
    code = generate_code(fmt)
    print(code)
