#!/usr/bin/env python3
import json
import argparse
import sys
import traceback
from typing import Any, Dict, List
import os
import subprocess as sp

# coding: utf-8

LANG: Dict[str, str] = {}


def format_lang(key: str, values: Dict[str, Any] = {}) -> str:
    return LANG[key].format(**values)


# --- 既存のクラス (変更なし) ---
class ValidationError(Exception):
    """カスタムの検証エラー例外"""

    pass


def ref_to_body(type_name: str):
    no_ref = type_name.removeprefix("Weak").removesuffix("Ref")
    if no_ref == "Identifier" or no_ref == "String":
        body_name = "String"
    else:
        body_name = no_ref + "Body"
    return body_name


def isArrayLikeObject(data: Any, field_map: dict):
    return isinstance(data, list) and not (
        set(field_map.keys()) - set({"len", "container"})
    )


class SchemaValidator:
    # (このクラスの実装は前回のものから変更ありません)
    """
    JSONオブジェクトが指定されたスキーマに準拠しているかを検証するクラス
    """

    def __init__(self, schema: Dict[str, Any]):
        """
        スキーマを前処理して、高速なルックアップのための辞書を準備します。
        """
        self.schema = schema
        # 構造体定義を名前でマッピング
        self.structs: Dict[str, Dict] = {
            s["name"]: s for s in schema.get("structs", [])
        }
        # Enum定義を名前でマッピングし、メンバー名をセットとして保持
        self.enums: Dict[str, Dict] = {e["name"]: e for e in schema.get("enums", [])}
        for name, enum_def in self.enums.items():
            enum_def["members_set"] = {m["name"] for m in enum_def["members"]}
        # subset_infoを構造体名とkindでマッピング
        self.subsets: Dict[str, Dict] = {
            s["name"]: {
                item["kind"]: item["fields"] for item in s["available_field_per_kind"]
            }
            for s in schema.get("subset_info", [])
        }

    def validate(
        self,
        data: Dict[str, Any],
        root_struct_name: str,
        rough: set[str] = set(),
        strict: bool = False,
    ):
        """
        検証プロセスのエントリーポイント
        """
        if root_struct_name not in self.structs:
            raise ValidationError(
                format_lang(
                    "root_struct_not_found", {"root_struct_name": root_struct_name}
                )
            )
        self._validate_object(
            data, root_struct_name, path=root_struct_name, rough=rough, strict=strict
        )

    def _validate_object(
        self, data: Any, struct_name: str, path: str, rough: set[str], strict: bool
    ):
        """
        オブジェクト（辞書）を再帰的に検証します。
        """
        if not strict and data is None:
            return  # nullの場合,この部分は比較対象外なので除外
        struct_def = self.structs[struct_name]
        field_map = {f["name"]: f for f in struct_def["fields"]}
        if not isinstance(data, dict):
            # もし配列ならばlenとcontainerに読み替える
            if isArrayLikeObject(data, field_map):
                data = {"len": len(data), "container": data}
            else:
                raise ValidationError(
                    format_lang(
                        "path_should_be_object", {"path": path, "type": type(data)}
                    )
                )

        active_field_names: set[str]
        subset_used = False
        if struct_name in self.subsets:
            kind_value = data.get("kind")
            if kind_value is None:
                raise ValidationError(
                    format_lang(
                        "path_should_have_kind",
                        {
                            "path": path,
                            "valid_values": ",".join(self.subsets[struct_name].keys()),
                        },
                    )
                )
            if kind_value not in self.subsets[struct_name]:
                raise ValidationError(
                    format_lang(
                        "invalid_kind_value",
                        {
                            "path": path,
                            "kind_value": kind_value,
                            "valid_values": ",".join(self.subsets[struct_name].keys()),
                        },
                    )
                )
            active_field_names = set(self.subsets[struct_name][kind_value])
            subset_used = True
            path = f"{path}({kind_value})"
        else:
            active_field_names = set(field_map.keys())

        extra_fields = set(data.keys()) - active_field_names
        if extra_fields:
            raise ValidationError(
                format_lang(
                    "invalid_field",
                    {"path": path, "extra_fields": ", ".join(extra_fields)},
                )
            )

        for field_name in sorted(list(active_field_names)):
            field_def = field_map[field_name]
            is_present = field_name in data
            is_pointer = (field_name in rough) or (
                not subset_used and field_def.get("is_pointer", False)
            )

            if not is_pointer and not is_present:
                raise ValidationError(
                    format_lang(
                        "missing_required_field",
                        {"path": path, "field_name": field_name},
                    )
                )

            if is_present:
                value = data[field_name]
                field_type = field_def["type"]
                is_array = field_def.get("is_array", False)
                new_path = f"{path}.{field_name}"

                if is_array:
                    if not isinstance(value, list):
                        raise ValidationError(
                            format_lang(
                                "path_should_be_array",
                                {"new_path": new_path, "type": type(value)},
                            )
                        )
                    for i, item in enumerate(value):
                        self._validate_value(
                            item, field_type, f"{new_path}[{i}]", rough, strict
                        )
                else:
                    self._validate_value(value, field_type, new_path, rough, strict)

    def _validate_value(
        self, value: Any, type_name: str, path: str, rough: set[str], strict: bool
    ):
        """
        与えられた型に基づいて個々の値を検証します。
        """
        if type_name in self.structs:
            self._validate_object(value, type_name, path, rough, strict)
        elif type_name in self.enums:
            valid_members = self.enums[type_name]["members_set"]
            if not isinstance(value, str) or value not in valid_members:
                raise ValidationError(
                    format_lang(
                        "not_enum_member",
                        {
                            "path": path,
                            "value": value,
                            "type_name": type_name,
                            "valid_values": ", ".join(sorted(list(valid_members))),
                        },
                    )
                )
        elif type_name.endswith("Ref") or type_name == "AnyRef":
            if isinstance(value, dict):
                body_name = ref_to_body(type_name)
                if body_name in self.structs:
                    self._validate_object(
                        value, body_name, f"{path}->{body_name}", rough, strict
                    )
                else:
                    raise ValidationError(
                        format_lang(
                            "type_cannot_have_object",
                            {"path": path, "type_name": type_name},
                        )
                    )
            elif not strict and value is None:
                pass  # nullの場合,この部分は比較対象外なので除外
            elif not isinstance(value, int):
                raise ValidationError(
                    format_lang(
                        "path_should_be_object_or_int",
                        {"path": path, "type": type(value)},
                    )
                )
        elif type_name in ("std::uint8_t", "std::uint64_t", "Varint"):
            if not isinstance(value, int):
                raise ValidationError(
                    format_lang(
                        "path_should_be_int", {"path": path, "type": type(value)}
                    )
                )
        elif type_name == "bool":
            if not isinstance(value, bool):
                raise ValidationError(
                    format_lang(
                        "path_should_be_bool", {"path": path, "type": type(value)}
                    )
                )
        elif type_name == "std::string":
            if not isinstance(value, str):
                raise ValidationError(
                    format_lang(
                        "path_should_be_string", {"path": path, "type": type(value)}
                    )
                )
        else:
            raise ValidationError(
                format_lang("unknown_type", {"path": path, "type_name": type_name})
            )


# --- ここから新規追加 ---
class EqualityError(Exception):
    """カスタムの等価性比較エラー例外"""

    pass


class EqualityTester:
    """
    テスト対象(T1)とテストケース(T2)のJSONオブジェクトの等価性を検証するクラス。
    T1のRef型はebm_mapを用いて解決する。
    """

    def __init__(
        self,
        validator: SchemaValidator,
        ebm_map: Dict[int, Any] | None = None,
        rough: set[str] = set(),
    ):
        self.validator = validator
        self.ebm_map = ebm_map if ebm_map is not None else {}
        self.rough_fields = rough

    def compare(self, target_obj: Any, case_obj: Any, struct_name: str):
        """比較プロセスのエントリーポイント"""
        self._compare_value(target_obj, case_obj, struct_name, path=struct_name)

    def _compare_value(self, t1_val: Any, t2_val: Any, type_name: str, path: str):
        """型に基づいて値を再帰的に比較する"""
        # 型が構造体の場合
        if type_name in self.validator.structs:
            self._compare_object(t1_val, t2_val, type_name, path)
            return

        # 型がRef系の場合 (特別ルール)
        if type_name.endswith("Ref"):
            self._compare_ref(t1_val, t2_val, type_name, path)
            return

        # その他のプリミティブ型やEnumの場合
        if t1_val != t2_val:
            raise EqualityError(
                format_lang(
                    "values_not_equal",
                    {"path": path, "t1_val": t1_val, "t2_val": t2_val},
                )
            )

    def _compare_ref(self, t1_ref: Any, t2_ref: Any, type_name: str, path: str):
        """Ref型を特別に比較する"""
        # T1のRefを解決する
        resolved_t1 = t1_ref
        if isinstance(t1_ref, int) and t1_ref in self.ebm_map:
            resolved_t1 = self.ebm_map[t1_ref]

        # T2がNoneの場合,比較をskip
        if t2_ref is None:
            return

        # T1(解決後)がオブジェクトの場合
        if isinstance(resolved_t1, dict):
            body_name = ref_to_body(type_name)
            if isinstance(t2_ref, dict):  # T2もオブジェクトなら再帰比較
                self._compare_object(
                    resolved_t1, t2_ref, body_name, f"{path}->{body_name}"
                )
            elif isinstance(t2_ref, int):  # T2が数値ならT1のIDと比較
                if t1_ref != t2_ref:
                    raise EqualityError(
                        format_lang(
                            "ids_not_equal",
                            {"path": path, "t1_ref": t1_ref, "t2_ref": t2_ref},
                        )
                    )
            else:
                raise EqualityError(
                    format_lang(
                        "type_mismatch_t1_object",
                        {
                            "path": path,
                            "t1_type": type(resolved_t1),
                            "t2_type": type(t2_ref),
                        },
                    )
                )
        # T1(解決後)が数値の場合
        elif isinstance(resolved_t1, int):
            if not isinstance(t2_ref, int):
                raise EqualityError(
                    format_lang(
                        "type_mismatch_t1_numeric",
                        {"path": path, "t1_val": resolved_t1, "t2_val": t2_ref},
                    )
                )
            if resolved_t1 != t2_ref:
                raise EqualityError(
                    format_lang(
                        "ref_values_not_equal",
                        {"path": path, "t1_val": resolved_t1, "t2_val": t2_ref},
                    )
                )
        else:
            raise EqualityError(format_lang("invalid_t1_value_type", {"path": path}))

    def _compare_object(self, t1_obj: Any, t2_obj: Any, struct_name: str, path: str):
        """オブジェクト（辞書）を再帰的に比較する"""
        if t2_obj is None:
            return  # nullの場合,この部分は比較対象外なので除外

        # もし配列ならばlenとcontainerに読み替える
        struct_def = self.validator.structs[struct_name]
        field_map = {f["name"]: f for f in struct_def["fields"]}
        if isArrayLikeObject(t2_obj, field_map):
            t2_obj = {"len": len(t2_obj), "container": t2_obj}
        if not isinstance(t1_obj, dict) or not isinstance(t2_obj, dict):
            raise EqualityError(
                format_lang(
                    "not_object",
                    {"path": path, "t1_type": type(t1_obj), "t2_type": type(t2_obj)},
                )
            )

        active_field_names: set[str]
        # subset_infoのハンドリング
        if struct_name in self.validator.subsets:
            t1_kind = t1_obj.get("kind")
            t2_kind = t2_obj.get("kind")
            if t1_kind != t2_kind:
                raise EqualityError(
                    format_lang(
                        "kind_mismatch",
                        {"path": path, "t1_kind": t1_kind, "t2_kind": t2_kind},
                    )
                )
            if t1_kind not in self.validator.subsets[struct_name]:
                raise EqualityError(
                    format_lang(
                        "kind_not_in_schema", {"path": path, "t1_kind": t1_kind}
                    )
                )
            active_field_names = set(self.validator.subsets[struct_name][t1_kind])
            path = f"{path}({t1_kind})"
        else:
            active_field_names = set(field_map.keys())

        # フィールドの比較
        for field_name in sorted(list(active_field_names)):
            field_def = field_map[field_name]
            t1_val = t1_obj.get(field_name)
            t2_val = t2_obj.get(field_name)

            new_path = f"{path}.{field_name}"
            field_type = field_def["type"]

            if (
                t1_val is not None
                and t2_val is None
                and (field_name in self.rough_fields)
            ):
                continue

            if field_def.get("is_array", False):
                if not isinstance(t1_val, list) or not isinstance(t2_val, list):
                    raise EqualityError(
                        format_lang("not_array", {"new_path": new_path})
                    )
                if len(t1_val) != len(t2_val):
                    raise EqualityError(
                        format_lang(
                            "array_length_mismatch",
                            {
                                "new_path": new_path,
                                "t1_len": len(t1_val),
                                "t2_len": len(t2_val),
                            },
                        )
                    )
                for i, (t1_item, t2_item) in enumerate(zip(t1_val, t2_val)):
                    self._compare_value(
                        t1_item, t2_item, field_type, f"{new_path}[{i}]"
                    )
            else:
                self._compare_value(t1_val, t2_val, field_type, new_path)


# --- 既存の関数 (変更なし) ---
def make_EBM_map(data: dict):
    map_target = ["identifiers", "strings", "types", "expressions", "statements"]
    result = {}
    for target in map_target:
        for element in data[target]:
            # RefのIDは数値なので、キーも数値に変換
            result[int(element["id"])] = element["body"]
    return result


sys.path.append(os.path.dirname(__file__))
from util import execute


# --- main関数を修正 ---
def main():
    """CLIのエントリーポイント"""
    LANG_ENV = os.environ.get("EBMTEST_LANG", "en")
    lang_path = os.path.join(
        os.path.dirname(__file__), f"ebmtest_lang_config.{LANG_ENV}.json"
    )
    with open(lang_path, "r", encoding="utf-8") as f:
        global LANG
        LANG.update(json.load(f))

    parser = argparse.ArgumentParser(description=LANG["cli_description"])
    parser.add_argument("test_data", help=LANG["test_data_help"])
    parser.add_argument("struct_name", help=LANG["struct_name_help"])
    parser.add_argument("--test-case", default=None, help=LANG["test_case_help"])
    parser.add_argument("--lang", default="en", help="言語設定 (en, ja)")
    args = parser.parse_args()

    if args.lang != LANG_ENV:
        lang_path = os.path.join(
            os.path.dirname(__file__), f"ebmtest_lang_config.{args.lang}.json"
        )
        with open(lang_path, "r", encoding="utf-8") as f:
            LANG.update(json.load(f))

    assert isinstance(args.test_data, str)
    assert isinstance(args.struct_name, str)
    assert isinstance(args.test_case, (str, type(None)))

    schema_json = json.loads(
        execute(["./tool/ebmcodegen", "--mode", "spec-json"], None, True)
    )

    try:
        # test_dataのsuffixが.ebmの場合、ebmgenでjson-ebmに変換
        if args.test_data.endswith(".ebm"):
            print(format_lang("converting_to_json_ebm", {"test_data": args.test_data}))
            data = execute(
                [
                    "./tool/ebmgen",
                    "-i",
                    args.test_data,
                    "--input-format",
                    "ebm",
                    "-d",
                    "-",
                    "--debug-format",
                    "json",
                ],
                None,
                True,
            )
        else:
            with open(args.test_data, "r", encoding="utf-8") as f:
                data = f.read()
        data_json = json.loads(data)
    except FileNotFoundError as e:
        print(format_lang("file_not_found", {"filename": e.filename}), file=sys.stderr)
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(format_lang("json_parse_error", {"error": e}), file=sys.stderr)
        sys.exit(1)

    try:
        validator = SchemaValidator(schema_json)
        print(
            format_lang(
                "validating_schema",
                {"test_data": args.test_data, "struct_name": args.struct_name},
            )
        )
        validator.validate(data_json, args.struct_name, strict=True)
        print(LANG["validation_success"])

        if args.test_case:
            # --- テストケース実行ロジック ---
            test_cases: list[str] = [args.test_case]
            if os.path.isdir(args.test_case):
                test_cases = [
                    os.path.join(args.test_case, f) for f in os.listdir(args.test_case)
                ]
            for case in test_cases:
                print("-" * 20)
                with open(case, "r", encoding="utf-8") as f:
                    test_case_json = json.load(f)

                query = test_case_json.get("query")
                if query and args.struct_name == "ExtendedBinaryModule":
                    print(LANG["filtering_data_with_query"])
                    print(format_lang("query", {"query": query}))
                    data = execute(
                        [
                            "./tool/ebmgen",
                            "query",
                            "-q",
                            query,
                            "--query-format",
                            "json",
                            "-i",
                            args.test_data,
                            "--timing",
                        ],
                        None,
                        True,
                    )
                    json_data = json.loads(data)
                    ids = [int(item["id"]) for item in json_data]
                    print(format_lang("extracted_ids", {"ids": ids}))
                else:
                    data = data.encode()

                # 1. T1 (テスト対象) をjqで抽出
                print(LANG["extracting_t1_with_jq"])
                print(
                    format_lang("condition", {"condition": test_case_json["condition"]})
                )
                target_t1 = json.loads(
                    execute(["jq", test_case_json["condition"]], None, True, data)
                )

                # 2. T2 (テストケース) を取得
                case_t2 = test_case_json["case"]
                struct_to_compare = test_case_json["struct"]
                rough_field = set[str](test_case_json.get("rough", []))

                # ---【復活させた検証部分 1/2】テストケース(T2)自体のスキーマ検証 ---
                print(
                    format_lang(
                        "validating_test_case_schema",
                        {
                            "test_case": args.test_case,
                            "struct_to_compare": struct_to_compare,
                        },
                    )
                )
                validator.validate(case_t2, struct_to_compare, rough_field)
                print(LANG["test_case_validation_success"])

                if not isinstance(target_t1, list):
                    target_t1 = [target_t1]

                for i, target_t1_instance in enumerate(target_t1):
                    print(
                        format_lang(
                            "validating_instance",
                            {"index": i + 1, "total": len(target_t1)},
                        )
                    )

                    # ---【復活させた検証部分 2/2】テスト対象(T1)のスキーマ検証 ---
                    print(
                        format_lang(
                            "validating_t1_schema",
                            {"struct_to_compare": struct_to_compare},
                        )
                    )
                    validator.validate(target_t1_instance, struct_to_compare)
                    print(LANG["t1_validation_success"])

                    # 3. ebm_mapを作成 (必要な場合)
                    ebm_map = None
                    if args.struct_name == "ExtendedBinaryModule":
                        ebm_map = make_EBM_map(data_json)

                    print(
                        format_lang(
                            "validating_equality", {"test_case": args.test_case}
                        )
                    )
                    verification_target_info = test_case_json["condition"]
                    if query:
                        verification_target_info = (
                            f"{query} -> {verification_target_info}"
                        )

                    verification_target_info += f" ({1 + i}/{len(target_t1)})"
                    print(
                        format_lang(
                            "t1_info",
                            {
                                "test_data": args.test_data,
                                "verification_target_info": verification_target_info,
                            },
                        )
                    )
                    print(format_lang("t2_info", {"test_case": args.test_case}))
                    print(
                        format_lang(
                            "excluded_fields", {"rough_field": ",".join(rough_field)}
                        )
                    )

                    # 4. EqualityTesterで比較
                    tester = EqualityTester(validator, ebm_map, rough_field)
                    tester.compare(target_t1_instance, case_t2, struct_to_compare)
                    print(LANG["equality_validation_success"])

    except (ValidationError, EqualityError) as e:
        print(format_lang("validation_failed", {"error": e}), file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(format_lang("unexpected_error", {"error": e}), file=sys.stderr)
        print(traceback.format_exc(), file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
