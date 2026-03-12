#!/usr/bin/env python3
import re
import sys
import struct


# --- AST ノード定義 ---
class Node:
    pass


class Field(Node):
    def __init__(self, name, type_name, length=None):
        self.name = name  # フィールド名
        self.type_name = type_name  # 型（例: uint8, byte など）
        self.length = length  # 配列の場合、角括弧内の式（例: length）


class Conditional(Node):
    def __init__(self):
        # branches は (condition, nodes) のタプルリスト
        # condition が None の場合は else ブランチ
        self.branches = []


class Format:
    def __init__(self, name, nodes):
        self.name = name  # フォーマット名（クラス名）
        self.nodes = nodes  # フィールドや条件分岐のリスト


# --- インデントヘルパー ---
def get_indent(line):
    # 先頭のスペース数をカウント（タブはスペースに変換しておく想定）
    return len(line) - len(line.lstrip(" "))


# --- 行単位のパーサー ---
def parse_field(line):
    # フィールド定義は "識別子 : 型" の形式
    # 型には配列指定子（例: byte[length]）も含む
    m = re.match(r"(\w+)\s*:\s*(\w+(?:\[[^\]]+\])?)", line)
    if not m:
        raise ValueError("フィールド定義のパースに失敗: " + line)
    name = m.group(1)
    type_str = m.group(2)
    # 型が配列指定子を含む場合の処理
    m2 = re.match(r"(\w+)\[([^\]]+)\]", type_str)
    if m2:
        type_name = m2.group(1)
        length = m2.group(2)
    else:
        type_name = type_str
        length = None
    return Field(name, type_name, length)


def parse_block(lines, start_index, current_indent):
    nodes = []
    i = start_index
    while i < len(lines):
        line = lines[i]
        # 空行やコメント行はスキップ
        if line.strip() == "" or line.strip().startswith("#"):
            i += 1
            continue
        indent = get_indent(line)
        if indent < current_indent:
            break
        if indent > current_indent:
            raise ValueError("予期しないインデントです: " + line)
        # 現在のインデントと一致
        stripped = line.strip()
        # 条件分岐のヘッダの場合
        if (
            stripped.startswith("if ")
            or stripped.startswith("else if")
            or stripped.startswith("else")
        ):
            cond_node, i = parse_conditional(lines, i, current_indent)
            nodes.append(cond_node)
        else:
            # フィールド定義の場合
            field = parse_field(stripped)
            nodes.append(field)
            i += 1
    return nodes, i


def parse_conditional(lines, start_index, current_indent):
    cond_node = Conditional()
    i = start_index
    # 現在のインデントレベルで、if / else if / else を連続して処理
    while i < len(lines):
        line = lines[i]
        if get_indent(line) != current_indent:
            break
        stripped = line.strip()
        if stripped.startswith("if "):
            condition = stripped[3:].strip()
        elif stripped.startswith("else if"):
            condition = stripped[len("else if") :].strip()
        elif stripped.startswith("else"):
            condition = None
        else:
            break
        i += 1
        # 条件ブランチの内部は current_indent より 4 スペース深いと仮定
        branch_nodes, i = parse_block(lines, i, current_indent + 4)
        cond_node.branches.append((condition, branch_nodes))
    return cond_node, i


def parse_dsl(dsl_text):
    lines = dsl_text.splitlines()
    # 空行を除去
    lines = [line for line in lines if line.strip() != ""]
    if not lines:
        raise ValueError("DSL が空です。")
    # 1行目は "format <Name>" で始まる必要がある
    header = lines[0].strip()
    m = re.match(r"format\s+(\w+)", header)
    if not m:
        raise ValueError("1行目は format 宣言である必要があります。")
    format_name = m.group(1)
    # 以降のブロックはインデント 4 スペースから開始すると仮定
    nodes, _ = parse_block(lines, 1, 4)
    return Format(format_name, nodes)


# --- コード生成部 ---
def get_struct_format(type_name):
    mapping = {
        "uint8": "B",
        "uint16": "H",
        "uint32": "I",
        "uint64": "Q",
        "float": "f",
        "double": "d",
        "byte": "c",
    }
    return mapping.get(type_name)


def collect_all_field_names(nodes):
    names = []
    for node in nodes:
        if isinstance(node, Field):
            names.append(node.name)
        elif isinstance(node, Conditional):
            for _, branch_nodes in node.branches:
                names.extend(collect_all_field_names(branch_nodes))
    # 重複除去（順序保持）
    seen = set()
    result = []
    for name in names:
        if name not in seen:
            seen.add(name)
            result.append(name)
    return result


def transform_condition_serialize(cond):
    # シリアライズ時、条件式内のフィールド参照に self. を付加（簡易実装）
    def repl(match):
        word = match.group(0)
        if word.isdigit() or word in {"and", "or", "not"} or word.startswith("self."):
            return word
        return "self." + word

    return re.sub(r"\b[a-zA-Z_]\w*\b", repl, cond)


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
                # 配列の場合：長さがリテラルの場合と、フィールド参照の場合で分岐
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
                        f"{indent}# ネストされた型のデシリアライズ: {node.name}"
                    )
                    lines.append(
                        f"{indent}{node.name} = {node.type_name}.deserialize(data[offset:])"
                    )
                    lines.append(f"{indent}# offset 更新が必要")
            else:
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
        elif isinstance(node, Conditional):
            first = True
            for cond, branch_nodes in node.branches:
                if cond is not None:
                    if first:
                        lines.append(f"{indent}if {cond}:")
                        first = False
                    else:
                        lines.append(f"{indent}elif {cond}:")
                else:
                    lines.append(f"{indent}else:")
                generate_nodes_deserialize(branch_nodes, lines, indent + "    ")


def generate_code(fmt: Format) -> str:
    lines = []
    lines.append("import struct")
    lines.append("")
    lines.append(f"class {fmt.name}:")
    all_fields = collect_all_field_names(fmt.nodes)
    if all_fields:
        params = ", ".join(all_fields)
        lines.append(f"    def __init__(self, {params}):")
        for name in all_fields:
            lines.append(f"        self.{name} = {name}")
    else:
        lines.append("    def __init__(self):")
        lines.append("        pass")
    lines.append("")
    lines.append("    def serialize(self):")
    lines.append("        parts = []")
    generate_nodes_serialize(fmt.nodes, lines, "        ")
    lines.append("        return b''.join(parts)")
    lines.append("")
    lines.append("    @classmethod")
    lines.append("    def deserialize(cls, data):")
    lines.append("        offset = 0")
    generate_nodes_deserialize(fmt.nodes, lines, "        ")
    cons_params = ", ".join(all_fields)
    lines.append(f"        return cls({cons_params})")
    return "\n".join(lines)


# --- メイン処理 ---
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("使い方: python dsl_codegen_indent.py <dsl_file>")
        sys.exit(1)
    dsl_file = sys.argv[1]
    with open(dsl_file, "r", encoding="utf-8") as f:
        dsl_text = f.read()
    fmt = parse_dsl(dsl_text)
    code = generate_code(fmt)
    print(code)
