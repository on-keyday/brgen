import json
import os
import sys
from typing import Any, Dict, Set

sys.path.append(os.path.dirname(__file__))
from util import execute


def ref_to_body(type_name: str) -> str:
    """Ref型名から対応するボディ型名を生成する。"""
    no_ref = type_name.removeprefix("Weak").removesuffix("Ref")
    if no_ref == "Identifier" or no_ref == "String":
        return "String"
    return no_ref + "Body"


def is_array_like_object(struct_def: Dict[str, Any]) -> bool:
    """カスタムスキーマの配列類似オブジェクトを判定する。"""
    fields = {f["name"] for f in struct_def["fields"]}
    return fields == {"len", "container"}


def convert_type(
    type_name: str, is_array: bool, structs: Set[str], enums: Set[str]
) -> Dict[str, Any]:
    """カスタム型定義をJSON Schemaの型定義に変換する"""
    schema = {}

    if is_array:
        schema["type"] = "array"
        schema["items"] = convert_type(type_name, False, structs, enums)
        return schema

    # 1. structsまたはenumsに定義が存在するかを優先的にチェック
    if type_name in structs or type_name in enums:
        schema["$ref"] = f"#/definitions/{type_name}"
        return schema

    # 2. Ref型であるかをチェック
    if type_name.endswith("Ref") or type_name == "AnyRef":
        body_name = ref_to_body(type_name)
        # 対応するbody型がstructsに存在する場合、埋め込みオブジェクトを許容
        if body_name in structs:
            schema["oneOf"] = [
                {"type": "integer", "description": f"Reference ID to a {body_name}"},
                {
                    "$ref": f"#/definitions/{body_name}",
                    "description": f"Inlined {body_name} object",
                },
            ]
        else:
            # Bodyが存在しない場合、IDのみを許容
            schema["type"] = "integer"
        return schema

    # 3. それ以外は、プリミティブ型として扱う
    if type_name in ["std::uint8_t", "std::uint64_t", "Varint"]:
        schema["type"] = "integer"
    elif type_name == "bool":
        schema["type"] = "boolean"
    elif type_name == "std::string":
        schema["type"] = "string"
    else:
        # 未知の型はエラーとする
        raise ValueError(f"Unknown type: {type_name}")

    return schema


def convert_struct(
    struct_def: Dict[str, Any],
    subset_info: Dict[str, Any],
    structs: Set[str],
    enums: Set[str],
) -> Dict[str, Any]:
    """一つの構造体定義をJSON Schemaに変換する"""
    if is_array_like_object(struct_def):
        field = next(f for f in struct_def["fields"] if f["name"] == "container")
        return {
            "type": "array",
            "items": convert_type(field["type"], False, structs, enums),
        }

    schema = {"type": "object", "properties": {}, "required": []}

    required_fields = []

    for field in struct_def["fields"]:
        field_name = field["name"]
        field_type = field["type"]
        is_array = field.get("is_array", False)
        is_pointer = field.get("is_pointer", False)

        prop_schema = convert_type(field_type, is_array, structs, enums)

        if struct_def["name"] == "ExtendedBinaryModule" and field_name == "magic":
            prop_schema["const"] = "EBMG"

        schema["properties"][field_name] = prop_schema

        if not is_pointer:
            required_fields.append(field_name)

    if struct_def["name"] in subset_info:
        subset = subset_info[struct_def["name"]]
        one_of_schemas = []

        if "kind" not in required_fields:
            required_fields.append("kind")

        for item in subset["available_field_per_kind"]:
            kind_value = item["kind"]
            fields_in_subset = item["fields"]

            then_schema = {"properties": {}, "required": []}

            for field_name in fields_in_subset:
                if field_name not in schema["properties"]:
                    continue

                then_schema["properties"][field_name] = schema["properties"][field_name]

                field_def = next(
                    f for f in struct_def["fields"] if f["name"] == field_name
                )
                if not field_def.get("is_pointer", False):
                    then_schema["required"].append(field_name)

            one_of_schemas.append(
                {
                    "if": {"properties": {"kind": {"const": kind_value}}},
                    "then": then_schema,
                }
            )

        schema = {
            "type": "object",
            "required": required_fields,
            "oneOf": one_of_schemas,
        }
    else:
        schema["required"] = required_fields

    return schema


def convert_enum(enum_def: Dict[str, Any]) -> Dict[str, Any]:
    """一つのEnum定義をJSON Schemaに変換する"""
    members = [m["name"] for m in enum_def["members"]]
    return {"type": "string", "enum": members}


def main():
    """カスタムスキーマを標準的なJSON Schemaに変換する"""

    custom_schema_data = json.loads(
        execute(["./tool/ebmcodegen", "--mode", "spec-json"], None, True)
    )
    json_schema = {
        "$schema": "http://json-schema.org/draft-07/schema#",
        "title": "Generated Schema from Custom Definition",
        "description": "JSON Schema generated from a custom ebmcodegen schema format.",
        "definitions": {},
    }

    struct_names = {s["name"] for s in custom_schema_data.get("structs", [])}
    enum_names = {e["name"] for e in custom_schema_data.get("enums", [])}
    subset_info_map = {s["name"]: s for s in custom_schema_data.get("subset_info", [])}

    # definitionsを先に構築
    for name in struct_names:
        json_schema["definitions"][name] = {}
    for name in enum_names:
        json_schema["definitions"][name] = {}

    # 実際に変換処理を呼び出す
    for struct_def in custom_schema_data.get("structs", []):
        name = struct_def["name"]
        json_schema["definitions"][name] = convert_struct(
            struct_def, subset_info_map, struct_names, enum_names
        )

    for enum_def in custom_schema_data.get("enums", []):
        name = enum_def["name"]
        json_schema["definitions"][name] = convert_enum(enum_def)

    print(json.dumps(json_schema, indent=2))


if __name__ == "__main__":
    main()
