#!/usr/bin/env python3
import re
import sys
import struct


# --- AST 定義 ---
class Field:
    def __init__(self, type_name, name, length=None):
        self.type_name = type_name  # 例: "uint8", "byte" など
        self.name = name  # フィールド名
        self.length = length  # 配列の場合、リテラルまたはフィールド参照（文字列）


class Format:
    def __init__(self, name, fields):
        self.name = name  # フォーマット名
        self.fields = fields  # Field のリスト


# --- DSL パーサー ---
def parse_dsl(dsl_text):
    # シンプルな正規表現によるパーサー（1ファイルに1フォーマットと仮定）
    fmt_pattern = r"format\s+(\w+)\s*{([^}]+)}"
    m = re.search(fmt_pattern, dsl_text, re.DOTALL)
    if not m:
        raise ValueError("DSL の形式が不正です。")
    fmt_name = m.group(1)
    fields_text = m.group(2)
    field_pattern = r"(\w+)\s+(\w+)(?:\[(\w+)\])?;"
    fields = []
    for m in re.finditer(field_pattern, fields_text):
        type_name = m.group(1)
        field_name = m.group(2)
        length = m.group(3)  # 存在しなければ None
        fields.append(Field(type_name, field_name, length))
    return Format(fmt_name, fields)


# --- 型から struct フォーマット指定子への変換 ---
def get_struct_format(type_name):
    mapping = {
        "uint8": "B",
        "uint16": "H",
        "uint32": "I",
        "uint64": "Q",
        "float": "f",
        "double": "d",
        # byte は後で特別扱い（文字列やバイト列として扱う）
        "byte": "c",
    }
    return mapping.get(type_name)


# --- コード生成 ---
def generate_code(fmt: Format) -> str:
    lines = []
    lines.append("import struct")
    lines.append("")
    lines.append(f"class {fmt.name}:")
    # コンストラクタ
    field_names = [f.name for f in fmt.fields]
    if field_names:
        params = ", ".join(field_names)
        lines.append(f"    def __init__(self, {params}):")
        for f in fmt.fields:
            lines.append(f"        self.{f.name} = {f.name}")
    else:
        lines.append("    def __init__(self):")
        lines.append("        pass")
    lines.append("")
    # シリアライズメソッド
    lines.append("    def serialize(self):")
    lines.append("        parts = []")
    for f in fmt.fields:
        # 固定長フィールドの場合
        if f.length is None:
            fmt_spec = get_struct_format(f.type_name)
            if fmt_spec:
                # byte 型の場合は 1 バイトの char として pack
                lines.append(
                    f"        parts.append(struct.pack('!{fmt_spec}', self.{f.name}))"
                )
            else:
                # ユーザー定義型の場合は、再帰的に serialize を呼び出す
                lines.append(f"        parts.append(self.{f.name}.serialize())")
        else:
            # 配列の場合
            # ※ここでは、f.length が数値リテラルの場合と、変数（例えば length フィールドの値）の場合とで処理を分けています。
            if f.length.isdigit():
                count = int(f.length)
                fmt_spec = get_struct_format(f.type_name)
                lines.append(f"        for item in self.{f.name}:")
                if fmt_spec:
                    lines.append(
                        f"            parts.append(struct.pack('!{fmt_spec}', item))"
                    )
                else:
                    lines.append(f"            parts.append(item.serialize())")
            else:
                # 変数長の場合（例：length フィールドで指定される）
                fmt_spec = get_struct_format(f.type_name)
                lines.append(f"        for item in self.{f.name}:")
                if fmt_spec:
                    lines.append(
                        f"            parts.append(struct.pack('!{fmt_spec}', item))"
                    )
                else:
                    lines.append(f"            parts.append(item.serialize())")
    lines.append("        return b''.join(parts)")
    lines.append("")
    # デシリアライズメソッド
    lines.append("    @classmethod")
    lines.append("    def deserialize(cls, data):")
    lines.append("        offset = 0")
    for f in fmt.fields:
        if f.length is None:
            fmt_spec = get_struct_format(f.type_name)
            if fmt_spec:
                size = struct.calcsize("!" + fmt_spec)
                lines.append(
                    f"        {f.name} = struct.unpack('!{fmt_spec}', data[offset:offset+{size}])[0]"
                )
                lines.append(f"        offset += {size}")
            else:
                lines.append(f"        # {f.name} はネストされた型と仮定")
                lines.append(
                    f"        {f.name} = {f.type_name}.deserialize(data[offset:])"
                )
                lines.append("        # ネスト先の長さ分 offset を更新する必要あり")
        else:
            # 配列の場合
            if f.length.isdigit():
                count = int(f.length)
                fmt_spec = get_struct_format(f.type_name)
                lines.append(f"        {f.name} = []")
                if fmt_spec:
                    size = struct.calcsize("!" + fmt_spec)
                    lines.append(f"        for _ in range({count}):")
                    lines.append(
                        f"            item = struct.unpack('!{fmt_spec}', data[offset:offset+{size}])[0]"
                    )
                    lines.append(f"            {f.name}.append(item)")
                    lines.append(f"            offset += {size}")
                else:
                    lines.append(f"        for _ in range({count}):")
                    lines.append(
                        f"            item = {f.type_name}.deserialize(data[offset:])"
                    )
                    lines.append(f"            {f.name}.append(item)")
                    lines.append(
                        "            # ネスト先の長さ分 offset を更新する必要あり"
                    )
            else:
                # 変数長の場合：f.length が変数名（例：前フィールドの値）と仮定
                fmt_spec = get_struct_format(f.type_name)
                lines.append(f"        {f.name} = []")
                lines.append(
                    f"        for _ in range(self.{f.length}):  # {f.length} は既に読み出したフィールドである前提"
                )
                if fmt_spec:
                    size = struct.calcsize("!" + fmt_spec)
                    lines.append(
                        f"            item = struct.unpack('!{fmt_spec}', data[offset:offset+{size}])[0]"
                    )
                    lines.append(f"            {f.name}.append(item)")
                    lines.append(f"            offset += {size}")
                else:
                    lines.append(
                        f"            item = {f.type_name}.deserialize(data[offset:])"
                    )
                    lines.append(f"            {f.name}.append(item)")
                    lines.append(
                        "            # ネスト先の長さ分 offset を更新する必要あり"
                    )
    # コンストラクタへ渡すパラメータは定義順と同じ順序とする
    param_list = ", ".join([f.name for f in fmt.fields])
    lines.append(f"        return cls({param_list})")
    return "\n".join(lines)


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
