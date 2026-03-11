#!/usr/bin/env python3
"""
EBM Inline Expansion Script

Ref型フィールドにオブジェクトが直接記述されている場合、
自動でIDを割り当てて適切なテーブルに追加し、
フラットなExtendedBinaryModule形式に変換する。

使用例:
  python script/ebm_inline_expand.py input.json -o output.json
  python script/ebm_inline_expand.py input.json --validate
"""

import json
import argparse
import sys
import os
from typing import Any, Dict, List, Optional, Set
from dataclasses import dataclass, field

sys.path.append(os.path.dirname(__file__))
from util import execute


@dataclass
class IdAllocator:
    """ID割り当て管理"""
    next_id: int = 1

    def allocate(self) -> int:
        id = self.next_id
        self.next_id += 1
        return id


@dataclass
class EBMTables:
    """EBMの各テーブル"""
    identifiers: List[Dict] = field(default_factory=list)
    strings: List[Dict] = field(default_factory=list)
    types: List[Dict] = field(default_factory=list)
    statements: List[Dict] = field(default_factory=list)
    expressions: List[Dict] = field(default_factory=list)
    aliases: List[Dict] = field(default_factory=list)


def ref_to_table_name(type_name: str) -> Optional[str]:
    """Ref型から対応するテーブル名を取得

    WeakStatementRef, LoweredStatementRef等はラッパー型なのでNoneを返す
    （これらはインライン展開の対象外）
    """
    mapping = {
        "StatementRef": "statements",
        "ExpressionRef": "expressions",
        "TypeRef": "types",
        "IdentifierRef": "identifiers",
        "StringRef": "strings",
    }
    return mapping.get(type_name)


def ref_to_body_struct(type_name: str) -> Optional[str]:
    """Ref型から対応するBody構造体名を取得

    WeakStatementRef, LoweredStatementRef等はラッパー型なのでNoneを返す
    （これらはインライン展開の対象外）
    """
    mapping = {
        "StatementRef": "StatementBody",
        "ExpressionRef": "ExpressionBody",
        "TypeRef": "TypeBody",
        "IdentifierRef": "String",
        "StringRef": "String",
    }
    return mapping.get(type_name)


def ref_to_id_field(type_name: str) -> str:
    """Ref型から対応するIDフィールド名を取得"""
    mapping = {
        "StatementRef": "StatementRef",
        "WeakStatementRef": "StatementRef",
        "ExpressionRef": "ExpressionRef",
        "TypeRef": "TypeRef",
        "IdentifierRef": "IdentifierRef",
        "StringRef": "StringRef",
        "LoweredStatementRef": "StatementRef",
        "LoweredExpressionRef": "ExpressionRef",
    }
    return mapping.get(type_name, type_name)


class InlineExpander:
    """インライン展開を行うクラス"""

    def __init__(self, schema: Dict[str, Any]):
        self.schema = schema
        self.structs: Dict[str, Dict] = {
            s["name"]: s for s in schema.get("structs", [])
        }
        self.enums: Dict[str, Set[str]] = {
            e["name"]: {m["name"] for m in e["members"]}
            for e in schema.get("enums", [])
        }
        self.subsets: Dict[str, Dict] = {
            s["name"]: {
                item["kind"]: item["fields"]
                for item in s["available_field_per_kind"]
            }
            for s in schema.get("subset_info", [])
        }

        self.allocator = IdAllocator()
        self.tables = EBMTables()

    def is_ref_type(self, type_name: str) -> bool:
        """Ref型かどうかを判定"""
        return type_name.endswith("Ref")

    def expand(self, data: Dict[str, Any], root_struct: str = "ExtendedBinaryModule") -> Dict[str, Any]:
        """
        インライン展開を実行し、ExtendedBinaryModule形式に変換

        入力JSONは以下の形式を想定:
        - statements, expressions, types, identifiers, strings の各配列
        - 各要素は {id: ..., body: ...} または body 直接
        - body内のRef型フィールドには数値IDまたはオブジェクトを指定可能
        """
        # 既存のテーブルデータを読み込み
        self._load_existing_tables(data)

        # 各テーブルを再帰的に展開
        for table_name in ["statements", "expressions", "types", "identifiers", "strings"]:
            table = getattr(self.tables, table_name)
            for i, item in enumerate(table):
                if "body" in item:
                    item["body"] = self._expand_value(
                        item["body"],
                        self._get_body_struct_name(table_name),
                        f"{table_name}[{i}].body"
                    )

        # 結果を構築
        return self._build_result()

    def _load_existing_tables(self, data: Dict[str, Any]):
        """既存のテーブルデータを読み込み、最大IDを更新"""
        max_id = 0

        for table_name in ["statements", "expressions", "types", "identifiers", "strings"]:
            table = getattr(self.tables, table_name)
            items = data.get(table_name, [])

            for item in items:
                if isinstance(item, dict):
                    if "id" in item:
                        item_id = item["id"]
                        max_id = max(max_id, item_id)
                        table.append(item)
                    elif "body" in item:
                        # IDがない場合は自動割り当て
                        new_id = self.allocator.allocate()
                        max_id = max(max_id, new_id)
                        table.append({"id": new_id, "body": item["body"]})
                    else:
                        # bodyもidもない場合、全体をbodyとして扱う
                        new_id = self.allocator.allocate()
                        max_id = max(max_id, new_id)
                        table.append({"id": new_id, "body": item})

        # aliasesも読み込み
        self.tables.aliases = data.get("aliases", [])

        # 最大IDを更新
        self.allocator.next_id = max(self.allocator.next_id, max_id + 1)

    def _get_body_struct_name(self, table_name: str) -> str:
        """テーブル名からBody構造体名を取得"""
        mapping = {
            "statements": "StatementBody",
            "expressions": "ExpressionBody",
            "types": "TypeBody",
            "identifiers": "String",
            "strings": "String",
        }
        return mapping[table_name]

    def _expand_value(self, value: Any, type_name: str, path: str) -> Any:
        """値を再帰的に展開"""
        if value is None:
            return None

        # Ref型の場合
        if self.is_ref_type(type_name):
            return self._expand_ref(value, type_name, path)

        # 構造体の場合
        if type_name in self.structs:
            return self._expand_object(value, type_name, path)

        # Enum型の場合
        if type_name in self.enums:
            return value  # そのまま返す

        # プリミティブ型の場合
        return value

    def _expand_ref(self, value: Any, type_name: str, path: str) -> Any:
        """Ref型フィールドを展開"""
        body_struct = ref_to_body_struct(type_name)
        table_name = ref_to_table_name(type_name)

        # ラッパー型（WeakStatementRef, LoweredStatementRef等）の場合
        # これらは {"id": N} 形式のオブジェクトで、インライン展開の対象外
        if body_struct is None:
            if isinstance(value, int):
                # 数値の場合は {"id": N} 形式に変換
                return {"id": value}
            if isinstance(value, dict):
                # 既に {"id": N} 形式の場合は構造体として展開
                return self._expand_object(value, type_name, path)
            raise ValueError(f"Invalid value for {type_name} at {path}: {type(value)}")

        # 通常のRef型（StatementRef, ExpressionRef等）の場合
        # 既に数値IDの場合はそのまま
        if isinstance(value, int):
            return value

        # オブジェクトの場合はインライン展開
        if isinstance(value, dict):
            if table_name is None:
                raise ValueError(f"Unknown ref type: {type_name} at {path}")

            # 新しいIDを割り当て
            new_id = self.allocator.allocate()

            # bodyを再帰的に展開
            expanded_body = self._expand_value(value, body_struct, f"{path}->")

            # テーブルに追加
            table = getattr(self.tables, table_name)
            table.append({"id": new_id, "body": expanded_body})

            return new_id

        raise ValueError(f"Invalid value for {type_name} at {path}: {type(value)}")

    def _expand_object(self, value: Any, struct_name: str, path: str) -> Dict[str, Any]:
        """構造体を展開"""
        if not isinstance(value, dict):
            # 配列形式の場合（Block等）
            if isinstance(value, list):
                struct_def = self.structs[struct_name]
                field_map = {f["name"]: f for f in struct_def["fields"]}
                if "container" in field_map and "len" in field_map:
                    # 配列形式を展開
                    container_type = field_map["container"]["type"]
                    expanded_container = [
                        self._expand_value(item, container_type, f"{path}[{i}]")
                        for i, item in enumerate(value)
                    ]
                    return {"len": len(expanded_container), "container": expanded_container}
            raise ValueError(f"Expected object for {struct_name} at {path}, got {type(value)}")

        struct_def = self.structs[struct_name]
        field_map = {f["name"]: f for f in struct_def["fields"]}
        result = {}

        # subset_infoがある場合、kindに応じて有効なフィールドを決定
        active_fields: Set[str]
        if struct_name in self.subsets:
            kind_value = value.get("kind")
            if kind_value is None:
                raise ValueError(f"Missing 'kind' field for {struct_name} at {path}")
            if kind_value not in self.subsets[struct_name]:
                raise ValueError(f"Invalid kind '{kind_value}' for {struct_name} at {path}")
            active_fields = set(self.subsets[struct_name][kind_value])
        else:
            active_fields = set(field_map.keys())

        # 各フィールドを処理
        for field_name, field_value in value.items():
            if field_name not in field_map:
                # 未知のフィールドは警告してスキップ（または厳密モードでエラー）
                print(f"Warning: Unknown field '{field_name}' at {path}", file=sys.stderr)
                continue

            field_def = field_map[field_name]
            field_type = field_def["type"]
            is_array = field_def.get("is_array", False)
            new_path = f"{path}.{field_name}"

            if is_array:
                if not isinstance(field_value, list):
                    raise ValueError(f"Expected array for {field_name} at {new_path}")
                result[field_name] = [
                    self._expand_value(item, field_type, f"{new_path}[{i}]")
                    for i, item in enumerate(field_value)
                ]
            else:
                result[field_name] = self._expand_value(field_value, field_type, new_path)

        return result

    def _build_result(self) -> Dict[str, Any]:
        """最終的なExtendedBinaryModule形式を構築"""
        return {
            "magic": "EBMG",
            "version": 0,
            "max_id": self.allocator.next_id - 1,
            "identifiers_len": len(self.tables.identifiers),
            "identifiers": self.tables.identifiers,
            "strings_len": len(self.tables.strings),
            "strings": self.tables.strings,
            "types_len": len(self.tables.types),
            "types": self.tables.types,
            "statements_len": len(self.tables.statements),
            "statements": self.tables.statements,
            "expressions_len": len(self.tables.expressions),
            "expressions": self.tables.expressions,
            "aliases_len": len(self.tables.aliases),
            "aliases": self.tables.aliases,
            "debug_info": {
                "len_files": 0,
                "files": [],
                "len_locs": 0,
                "locs": []
            }
        }


def load_schema() -> Dict[str, Any]:
    """ebmcodegenからスキーマを取得"""
    output = execute(["./tool/ebmcodegen", "--mode", "spec-json"], None, True)
    return json.loads(output)


def main():
    parser = argparse.ArgumentParser(
        description="EBM Inline Expansion - Ref型フィールドのインライン記述を展開"
    )
    parser.add_argument("input", help="入力JSONファイル")
    parser.add_argument("-o", "--output", help="出力JSONファイル（省略時は標準出力）")
    parser.add_argument("--validate", action="store_true", help="展開後のJSONをebmtestで検証")
    parser.add_argument("--pretty", action="store_true", help="整形して出力")
    args = parser.parse_args()

    # スキーマを読み込み
    try:
        schema = load_schema()
    except Exception as e:
        print(f"Error loading schema: {e}", file=sys.stderr)
        sys.exit(1)

    # 入力JSONを読み込み
    try:
        with open(args.input, "r", encoding="utf-8") as f:
            input_data = json.load(f)
    except FileNotFoundError:
        print(f"Error: File not found: {args.input}", file=sys.stderr)
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON: {e}", file=sys.stderr)
        sys.exit(1)

    # インライン展開を実行
    try:
        expander = InlineExpander(schema)
        result = expander.expand(input_data)
    except Exception as e:
        print(f"Error during expansion: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)

    # 出力
    indent = 2 if args.pretty else None
    output_json = json.dumps(result, indent=indent, ensure_ascii=False)

    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(output_json)
        print(f"Output written to: {args.output}")
    else:
        print(output_json)

    # 検証
    if args.validate:
        print("\nValidating with ebmtest...", file=sys.stderr)
        # TODO: ebmtestを呼び出して検証


if __name__ == "__main__":
    main()
