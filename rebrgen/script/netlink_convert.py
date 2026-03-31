import os
import pathlib
import yaml
import glob
import re
from typing import Dict, Any, List
import sys
sys.path.append(pathlib.Path(__file__).resolve().parent.as_posix())
from extract_enum import EnumExtractor,get_clang_ast_json


class NetlinkDslGenerator:
    def __init__(self, spec_dir: str):
        self.spec_dir = spec_dir
        self.families: Dict[str, Any] = {}
        self.type_map = {
            "enum": "u32",
            "flags": "u32",
            "u8": "u8",
            "u16": "u16",
            "u32": "u32",
            "u64": "u64",
            "s32": "i32",
            "s64": "i64",
            "uint": "common.uint",
            "string": "common.string",
            "binary": "common.binary",
            "flag": "common.flag",
            "pad": "common.pad"
        }

    def _safe_name(self, name: str) -> str:
        if not name: return "unknown"
        if name in ["config", "input","output","state"]:
            return f"{name}_"
        return name.replace("-", "_")

    def parse_specs(self):
        """YAMLを読み込み、内部構造を整理する"""
        yaml_files = glob.glob(os.path.join(self.spec_dir, "**/*.yaml"), recursive=True)
        for file_path in yaml_files:
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    data = yaml.safe_load(f)
                if not isinstance(data, dict) or 'name' not in data: continue
                
                family_name = data['name']
                # subset-of 解決用に、名前で引ける生のセットリストを保持
                data['raw_attribute_sets'] = {s['name']: s for s in data.get('attribute-sets', [])}
                self.families[family_name] = data
            except Exception as e:
                print(f"# Error parsing {file_path}: {e}")

    def resolve_all_subsets(self):
        """全ファミリーの全セットに対して subset-of を解決する"""
        for f_name, f_data in self.families.items():
            # attribute-sets そのものはリストなので、辞書形式の raw から辿る
            raw_sets = f_data.get('raw_attribute_sets', {})
            
            # 各セットを走査
            for s in f_data.get('attribute-sets', []):
                set_name = s['name']
                base_set_name = raw_sets.get(set_name, {}).get('subset-of')
                
                if base_set_name:
                    self._apply_subset_definition(s, base_set_name, raw_sets)

    def _apply_subset_definition(self, target_set: dict, base_set_name: str, raw_sets: dict):
        """ベースとなるセットから不足している定義（type, value等）をマージする"""
        base_set = raw_sets.get(base_set_name)
        if not base_set: return

        base_attrs = {a['name']: a for a in base_set.get('attributes', [])}
        
        for attr in target_set.get('attributes', []):
            attr_name = attr['name']
            if attr_name in base_attrs:
                base_info = base_attrs[attr_name]
                
                # 1. 型 (type) / ID (value) の継承
                for key in ['type', 'value', 'nested-attributes', 'enum', 'enum-as-flags', 'struct']:
                    if key not in attr and key in base_info:
                        attr[key] = base_info[key]

    def predefined(self):
        return """
input.endian = config.endian.native

format uint:
    data :u32 # 本来はサイズ可変だが簡略化

format string:
    data :[..]u8
    # :"\\0" # 生成の都合上以下で対応
    data.length >= 1 && data[data.length - 1] == 0

format unused:
    data :[..]u8

format flag:
    data :u32

format binary:
    data :[..]u8

format pad:
    data :[..]u8    
"""

    def generate_dsl(self, family_name: str) -> str:
        # 生成直前に全ての継承関係を解決
        self.resolve_all_subsets()
        
        spec = self.families.get(family_name)
        if not spec: return f"# Family {family_name} not found"
        uapi_header = spec.get("uapi-header",f"linux/{family_name}.h")
        clang_ast = get_clang_ast_json(uapi_header)
        output = []
        if clang_ast:
            enum_extractor = EnumExtractor()
            enum_extractor.extract_enums(clang_ast)

            # まずとりあえず AST から抽出した enum を出力してみる
            for enum in enum_extractor.results:
                output.append(f"enum {enum['name']}:")
                for member in enum['members']:
                    output.append(f"  {member['name']} = {member['value']}")


        output.append(f"# Generated DSL for {family_name.upper()}")
        output.append(f"common ::= config.import(\"common.bgn\")\n")
        
        if 'definitions' in spec:
            for d in spec['definitions']:
                if d.get('type') in ['enum', 'flags']:
                    output.append(self._gen_enum(d))
                elif d.get('type') == 'struct':
                    output.append(self._gen_struct(d))

        if 'attribute-sets' in spec:
            for s in spec['attribute-sets']:
                output.append(self._gen_attr_set(s))

        return "\n".join(output)

    def _gen_enum(self, d: dict) -> str:
        name = self._safe_name(d['name'])
        raw_type = d.get('type', '')
        base_type = self.type_map.get(raw_type, raw_type if raw_type != "enum" else "")
        
        type_line = f"\n    :{base_type}" if base_type else ""
        lines = [f"enum {name}:{type_line}"]
        
        entries = d.get('entries', [])
        if len(entries) == 0: entries = ["unknown"]
        
        for i, entry in enumerate(entries):
            if isinstance(entry, dict):
                e_name = self._safe_name(entry['name'])
                e_val = entry.get('value', i)
                lines.append(f"    {e_name} = {e_val}")
            else:
                lines.append(f"    {self._safe_name(entry)} = {i}")
        return "\n".join(lines) + "\n"

    def _gen_struct(self, d: dict) -> str:
        lines = [f"format {self._safe_name(d['name'])}:"]
        for m in d.get('members', []):
            m_name = self._safe_name(m['name'])
            type_name = self.type_map.get(m.get('type'), m.get('type'))
            m_type = self._safe_name(m.get('enum') or m.get('enum-as-flags') or type_name)
            lines.append(f"    {m_name} :{m_type}")
        return "\n".join(lines) + "\n"

    def _gen_attr_set(self, s: dict) -> str:
        set_name = self._safe_name(s['name'])
        extra_formats = []
        attr_type = f"{set_name}_AttrType"
        attr_enums = [
            f"enum {attr_type}:",
            "    :u16"
        ]
        lines = [f"format {set_name}_Attr:"]
        lines.append("    len :u16")
        lines.append(f"    type :{attr_type}")
        lines.append("    len >= 4")
        lines.append("    match type:")
        
        val_idx = 1
        for attr in s.get('attributes', []):
            # resolve_all_subsets によって ID(value) や型が補完されている
            v = attr.get('value', val_idx)
            val_idx = v + 1
            a_name = self._safe_name(attr['name'])
            attr_enums.append(f"    {a_name.upper()} = {v}")
            tag = "{}.{}".format(attr_type,a_name.upper() )
            a_type = attr.get('type')
            # mapping type
            a_type = self.type_map.get(a_type, a_type)
            if a_type == 'indexed-array':
                # 要素用の型名を決定
                sub_type = attr.get('sub-type')
                if sub_type in ['nest', 'nested']:
                    target_type = f"{self._safe_name(attr['nested-attributes'])}_Attr"
                else:
                    target_type = self._safe_name(sub_type)
                
                elem_format_name = f"{a_name}_Element"
                # 親の match 分岐には繰り返しとして定義
                lines.append(f"        {tag}:")
                lines.append(f"            {a_name} :[..]{elem_format_name}(input = input.subrange(len - 4))")
                
                # 要素用の format を定義（後で output に追加）
                elem_code = [
                    f"format {elem_format_name}:",
                    "    len :u16",
                    "    index :u16", # nla_type を index として解釈
                    "    len >= 4",
                    f"    data :{target_type}(input = input.subrange(len - 4))",
                    "    align :[..]u8(input.align = 32)\n"
                ]
                extra_formats.append("\n".join(elem_code))
            else:
                if a_type in ['nest', 'nested','nest-type-value']:
                    target = f"{self._safe_name(attr['nested-attributes'])}_Attr"
                elif a_type == 'struct':
                    target = self._safe_name(attr['struct'])
                else:
                    target = self._safe_name(attr.get('enum') or attr.get('enum-as-flags') or a_type)

                lines.append(f"        {tag}: ")
                lines.append(f"            {a_name} :{target}(input = input.subrange(len - 4))")
        lines.append("        .. => unknown :common.unused(input = input.subrange(len - 4))")
        lines.append("    align :[..]u8(input.align = 32)")

        lines.append("") # 最後に改行を入れる
        if len(s.get('attributes', [])) == 0:
            attr_enums.append("    UNKNOWN = 0") # 属性が空の場合は UNKNOWN を入れておく
        attr_enums.append("") # 最後に改行を入れる
        return "\n".join(attr_enums + lines + extra_formats) + "\n"




if __name__ == "__main__":
    import codecs 
    sys.stdout = codecs.getwriter("utf-8")(sys.stdout.buffer)
    # 使用法: 
    # 1. linux kernel のソースを clone する
    # 2. 以下のパスを Documentation/netlink/specs に書き換えて実行
    # 例: "/path/to/linux/Documentation/netlink/specs"
    SPEC_DIR = "save/linux/Documentation/netlink/" 
    resolver = NetlinkDslGenerator(SPEC_DIR)
    print(f"# Source specs loaded from {SPEC_DIR}")
    # 2. 全データの取得
    resolver.parse_specs()
    
    DST_DIR = "save/netlink_dsls"
    os.makedirs(DST_DIR, exist_ok=True)
    # predefined 
    with open(os.path.join(DST_DIR, f"common.bgn"), "w", encoding="utf-8") as f:
        f.write(resolver.predefined())
    # 3. 各ファミリーごとに DSL を生成して保存
    for family in resolver.families.keys():
        dsl = resolver.generate_dsl(family)
        with open(os.path.join(DST_DIR, f"{family}.bgn"), "w", encoding="utf-8") as f:
            f.write(dsl)
    merged_path = os.path.join(DST_DIR, f"netlink.bgn")
    # <sanitized file name> ::= config.import("<file name>") を並べたファイルを生成
    with open(merged_path, "w", encoding="utf-8") as f:
        for family in resolver.families.keys():
            f.write(f'{resolver._safe_name(family)} ::= config.import("{family}.bgn")\n')
    print(f"# DSL generation completed for {len(resolver.families)} families.")
    print(f"# DSL files generated in {DST_DIR}")