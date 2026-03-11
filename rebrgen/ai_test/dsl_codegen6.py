#!/usr/bin/env python3
import re
import sys
import struct


# --- AST ノード定義 ---
class Node:
    pass


class Field(Node):
    def __init__(self, name, type_name, length=None, bit_width=None):
        self.name = name  # フィールド名
        self.type_name = type_name  # 型（例: u8, u16, u32, byte など）
        self.length = length  # 配列の場合、角括弧内の式（例: length）
        self.bit_width = bit_width  # ビットフィールドの場合、その幅（例: 3）


class Conditional(Node):
    def __init__(self):
        # branches は (condition, nodes) のタプルリスト
        # condition が None の場合は else ブランチ
        self.branches = []


class Format:
    def __init__(self, name, nodes):
        self.name = name  # フォーマット名（クラス名／構造体名）
        self.nodes = nodes  # フィールドや条件分岐のリスト


# --- インデント・パーサー（DSL はインデントベース、後置記法） ---
def get_indent(line):
    return len(line) - len(line.lstrip(" "))


def parse_field(line):
    # フィールド定義は "識別子 : 型" の形式
    # 型は以下のいずれか:
    # 1. 単純な型（例: u8 または u8:3 でビットフィールド）
    # 2. 配列の場合は "[長さ]型"（ビットフィールドとの併用は想定しない）
    m = re.match(r"(\w+)\s*:\s*(\S+)", line)
    if not m:
        raise ValueError("フィールド定義のパースに失敗: " + line)
    name = m.group(1)
    type_str = m.group(2)
    # 配列の場合: 先頭が [ なら
    if type_str.startswith("["):
        m_array = re.match(r"\[([^\]]+)\](\w+)", type_str)
        if not m_array:
            raise ValueError("配列フィールドのパースに失敗: " + line)
        length = m_array.group(1)
        type_name = m_array.group(2)
        return Field(name, type_name, length=length)
    else:
        # ビットフィールドをサポート：型の後にコロンと数字があればビット幅
        m_bit = re.match(r"(\w+)(?::(\d+))?$", type_str)
        if not m_bit:
            raise ValueError("型定義のパースに失敗: " + line)
        type_name = m_bit.group(1)
        bit_width = m_bit.group(2)  # None または数字（文字列）
        return Field(name, type_name, bit_width=bit_width)


def parse_block(lines, start_index, current_indent):
    nodes = []
    i = start_index
    while i < len(lines):
        line = lines[i]
        if line.strip() == "" or line.strip().startswith("#"):
            i += 1
            continue
        indent = get_indent(line)
        if indent < current_indent:
            break
        if indent > current_indent:
            raise ValueError("予期しないインデントです: " + line)
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
            field = parse_field(stripped)
            nodes.append(field)
            i += 1
    return nodes, i


def parse_conditional(lines, start_index, current_indent):
    cond_node = Conditional()
    i = start_index
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
        # ブランチ内は current_indent より 4 スペース深いと仮定
        branch_nodes, i = parse_block(lines, i, current_indent + 4)
        cond_node.branches.append((condition, branch_nodes))
    return cond_node, i


def parse_dsl(dsl_text):
    lines = dsl_text.splitlines()
    lines = [line for line in lines if line.strip() != ""]
    if not lines:
        raise ValueError("DSL が空です。")
    # 1行目は "format <Name>" である必要がある
    header = lines[0].strip()
    m = re.match(r"format\s+(\w+)", header)
    if not m:
        raise ValueError("1行目は format 宣言である必要があります。")
    format_name = m.group(1)
    nodes, _ = parse_block(lines, 1, 4)
    return Format(format_name, nodes)


def collect_all_fields(nodes):
    fields = []
    for node in nodes:
        if isinstance(node, Field):
            fields.append(node)
        elif isinstance(node, Conditional):
            for _, branch_nodes in node.branches:
                fields.extend(collect_all_fields(branch_nodes))
    # 重複除去（フィールド名で判定、定義順は保持）
    seen = set()
    result = []
    for field in fields:
        if field.name not in seen:
            seen.add(field.name)
            result.append(field)
    return result


# --- Python コード生成部 ---
def get_struct_format(type_name):
    mapping = {
        "u8": "B",
        "u16": "H",
        "u32": "I",
        "u64": "Q",
        "f32": "f",
        "f64": "d",
        "byte": "c",
    }
    return mapping.get(type_name)


def transform_condition_serialize(cond):
    # シリアライズ時、条件式中のフィールド参照に self. を付加（簡易実装）
    def repl(match):
        word = match.group(0)
        if word.isdigit() or word in {"and", "or", "not"} or word.startswith("self."):
            return word
        return "self." + word

    return re.sub(r"\b[a-zA-Z_]\w*\b", repl, cond)


def generate_nodes_serialize(nodes, lines, indent):
    i = 0
    while i < len(nodes):
        node = nodes[i]
        # --- ビットフィールドのグループ処理 ---
        if isinstance(node, Field) and node.bit_width is not None:
            group = []
            base_type = node.type_name
            while (
                i < len(nodes)
                and isinstance(nodes[i], Field)
                and nodes[i].bit_width is not None
                and nodes[i].type_name == base_type
            ):
                group.append(nodes[i])
                i += 1
            total_bits = sum(int(f.bit_width) for f in group)
            type_sizes = {"u8": 8, "u16": 16, "u32": 32, "u64": 64}
            if base_type not in type_sizes:
                raise ValueError("Unsupported bit field base type: " + base_type)
            base_size = type_sizes[base_type]
            if total_bits > base_size:
                raise ValueError(
                    "Bit field group total bits exceed capacity for type " + base_type
                )
            fmt_spec = get_struct_format(base_type)
            lines.append(f"{indent}bit_group = 0")
            for field in group:
                bw = int(field.bit_width)
                lines.append(
                    f"{indent}bit_group = (bit_group << {bw}) | (self.{field.name} & ((1 << {bw}) - 1))"
                )
            lines.append(f"{indent}parts.append(struct.pack('!{fmt_spec}', bit_group))")
            continue
        # --- 通常フィールドまたは配列フィールド ---
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
                fmt_spec = get_struct_format(node.type_name)
                lines.append(f"{indent}for item in self.{node.name}:")
                if fmt_spec:
                    lines.append(
                        f"{indent}    parts.append(struct.pack('!{fmt_spec}', item))"
                    )
                else:
                    lines.append(f"{indent}    parts.append(item.serialize())")
            i += 1
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
            i += 1
        else:
            i += 1


def generate_nodes_deserialize(nodes, lines, indent):
    i = 0
    while i < len(nodes):
        node = nodes[i]
        # --- ビットフィールドのグループ処理 ---
        if isinstance(node, Field) and node.bit_width is not None:
            group = []
            base_type = node.type_name
            while (
                i < len(nodes)
                and isinstance(nodes[i], Field)
                and nodes[i].bit_width is not None
                and nodes[i].type_name == base_type
            ):
                group.append(nodes[i])
                i += 1
            total_bits = sum(int(f.bit_width) for f in group)
            type_sizes = {"u8": 8, "u16": 16, "u32": 32, "u64": 64}
            if base_type not in type_sizes:
                raise ValueError("Unsupported bit field base type: " + base_type)
            base_size = type_sizes[base_type]
            if total_bits > base_size:
                raise ValueError(
                    "Bit field group total bits exceed capacity for type " + base_type
                )
            fmt_spec = get_struct_format(base_type)
            size = struct.calcsize("!" + fmt_spec)
            lines.append(
                f"{indent}bit_group = struct.unpack('!{fmt_spec}', data[offset:offset+{size}])[0]"
            )
            lines.append(f"{indent}offset += {size}")
            lines.append(f"{indent}remaining = {total_bits}")
            for field in group:
                bw = int(field.bit_width)
                lines.append(
                    f"{indent}{field.name} = (bit_group >> (remaining - {bw})) & ((1 << {bw}) - 1)"
                )
                lines.append(f"{indent}remaining -= {bw}")
            continue
        # --- 通常フィールドまたは配列フィールド ---
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
            i += 1
        elif isinstance(node, Conditional):
            for cond, branch_nodes in node.branches:
                if cond is not None:
                    lines.append(f"{indent}if {cond}:")
                else:
                    lines.append(f"{indent}else:")
                generate_nodes_deserialize(branch_nodes, lines, indent + "    ")
            i += 1
        else:
            i += 1


def generate_python_code(fmt: Format) -> str:
    lines = []
    lines.append("import struct")
    lines.append("")
    lines.append(f"class {fmt.name}:")
    all_fields = [f.name for f in collect_all_fields(fmt.nodes)]
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


# --- C++ コード生成部 ---
def generate_cpp_code(fmt: Format) -> str:
    lines = []
    lines.append("#include <cstdint>")
    lines.append("#include <vector>")
    lines.append("")
    lines.append(f"struct {fmt.name} " + "{")
    fields = collect_all_fields(fmt.nodes)
    type_map = {
        "u8": "uint8_t",
        "u16": "uint16_t",
        "u32": "uint32_t",
        "u64": "uint64_t",
        "f32": "float",
        "f64": "double",
        "byte": "uint8_t",
    }
    for field in fields:
        ctype = type_map.get(field.type_name, field.type_name)
        if field.length is not None:
            lines.append(
                f"    std::vector<{ctype}> {field.name};  // size: {field.length}"
            )
        elif field.bit_width is not None:
            lines.append(f"    {ctype} {field.name} : {field.bit_width};")
        else:
            lines.append(f"    {ctype} {field.name};")
    lines.append("")
    lines.append("    // シリアライズ（実装例は省略）")
    lines.append("    std::vector<uint8_t> serialize() const {")
    lines.append("        // TODO: 実装を追加")
    lines.append("        return std::vector<uint8_t>();")
    lines.append("    }")
    lines.append("")
    lines.append(
        f"    static {fmt.name} deserialize(const std::vector<uint8_t>& data) " + "{"
    )
    lines.append(f"        {fmt.name} obj;")
    lines.append("        // TODO: 実装を追加")
    lines.append("        return obj;")
    lines.append("    }")
    lines.append("};")
    return "\n".join(lines)


# --- メイン処理 ---
if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python dsl_codegen_indent.py <dsl_file> --target=<python|cpp>")
        sys.exit(1)
    dsl_file = sys.argv[1]
    target = "python"
    for arg in sys.argv[2:]:
        if arg.startswith("--target="):
            target = arg.split("=")[1]
    with open(dsl_file, "r", encoding="utf-8") as f:
        dsl_text = f.read()
    fmt = parse_dsl(dsl_text)
    if target == "python":
        code = generate_python_code(fmt)
    elif target == "cpp":
        code = generate_cpp_code(fmt)
    else:
        print("Unknown target language:", target)
        sys.exit(1)
    print(code)
