from __future__ import annotations
import yaml
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Union, Any, cast
import os
from pathlib import Path
import re


@dataclass
class KsySwitch:
    target: str
    cases: Dict[str, dict[str, Any]]


# kaitaiはu1 = 1バイトだがこちらはu8 = 1バイトなので、kaitaiの型をBGNの型に変換する関数
# u1, u2, u4, u8 はそれぞれ 1, 2, 4, 8 バイトの整数を表すため、u1 -> u8, u2 -> u16, u4 -> u32, u8 -> u64 に変換する
# b1, b2, b4 などは u1, u2, u4 に変換する
INTEGER_TYPE_REGEX = re.compile(r"^(u|s)(\d+)(le|be)?$")
FLOAT_TYPE_REGEX = re.compile(r"^f(\d+)(le|be)?$")
BIT_TYPE_REGEX = re.compile(r"^b(\d+)$")


def kaitai_type_to_bgn_type(ksy_type: str) -> str:
    # uNやsNは、Nをバイト数とする整数型を表すため、u8, u16, u32, u64などに変換する
    integer_match = INTEGER_TYPE_REGEX.match(ksy_type)
    if integer_match:
        prefix = integer_match.group(1)
        size = int(integer_match.group(2)) * 8
        # le,be は ub, ul などに変換する
        if ksy_type.endswith("le"):
            prefix += "l"
        elif ksy_type.endswith("be"):
            prefix += "b"
        if prefix == "s":
            prefix = "i"  # signed integer は i に変換
        return f"{prefix}{size}"
    # fNは、Nをバイト数とする浮動小数点数型を表すため、f32やf64などに変換する
    float_match = FLOAT_TYPE_REGEX.match(ksy_type)
    if float_match:
        size = int(float_match.group(1)) * 8
        if ksy_type.endswith("le"):
            return f"fl{size}"
        elif ksy_type.endswith("be"):
            return f"fb{size}"
        return f"f{size}"
    bit_type_match = BIT_TYPE_REGEX.match(ksy_type)
    if bit_type_match:
        size = int(bit_type_match.group(1))
        return f"u{size}"
    if ksy_type == "str":
        return "u8"  # 文字列はバイト列として扱う
    # それ以外の型はそのまま返す
    return ksy_type


def kaitai_expr_to_bgn_expr(expr: str) -> str:
    # Kaitai Struct の式を BGN の式に変換するための関数
    # ここでは、Kaitai Struct の式をそのまま返すだけにしておく
    # 必要に応じて、Kaitai Struct の式を BGN の式に変換するロジックを追加することができる
    # ~ は ! に変換する
    expr = str(expr)
    expr = expr.replace("~", "!")
    # :: は . に変換する
    expr = expr.replace("::", ".")
    # _io.pos は input.offset に変換する
    expr = expr.replace("_io.pos", "input.offset")
    # and or は &&, || に変換する
    expr = expr.replace(" and ", " && ")
    expr = expr.replace(" or ", " || ")
    # <ident>._sizeof は sizeof(<ident>) に変換する
    expr = re.sub(r"(\w+)\._sizeof", r"sizeof(\1)", expr)
    # 0b1000_0000_0000_0000 のようなバイナリリテラルは、0b1000000000000000 のようにアンダースコアを削除する
    expr = re.sub(r"0b([01_]+)", lambda m: "0b" + m.group(1).replace("_", ""), expr)
    return expr


@dataclass
class KsyAttribute:
    id: str
    # type は 単なる文字列(型名) か、match式(KsySwitch) のいずれか
    type: Optional[Union[str, KsySwitch]] = None
    size: Optional[Union[int, str]] = None
    if_expr: Optional[str] = None

    @classmethod
    def from_dict(cls, d: Dict[str, Any]) -> KsyAttribute:
        raw_type = d.get("type")
        final_type: Optional[Union[str, KsySwitch]] = None

        if isinstance(raw_type, str):
            final_type = kaitai_type_to_bgn_type(raw_type)
        elif isinstance(raw_type, dict):
            # switch-on がある場合のみ KsySwitch に変換
            size = d.get("size")
            if size is not None and isinstance(size, str):
                size = kaitai_expr_to_bgn_expr(size)
                size = "(input = input.subrange({}))".format(size)
            else:
                size = ""
            if "switch-on" in raw_type:
                final_type = KsySwitch(
                    target=cast(str, raw_type["switch-on"]),
                    cases={
                        kaitai_expr_to_bgn_expr(k): {
                            "key": "var" + v,
                            "type": (kaitai_type_to_bgn_type(str(v)) + size),
                        }
                        for k, v in raw_type.get("cases", {}).items()
                    },
                )
        else:
            final_type = "u8"

        if d.get("enum") is not None:
            final_type = d["enum"] + "(config.type = {})".format(final_type)

        # sizeがある場合final_typeがu8なら[size]u8
        if d.get("size") is not None and final_type == "u8":
            size = kaitai_expr_to_bgn_expr(str(d["size"]))
            final_type = f"[{size}]{final_type}"

        # repeat-exprの場合[expr]Typeの形式にする
        if "repeat-expr" in d:
            repeat_expr = kaitai_expr_to_bgn_expr(cast(str, d["repeat-expr"]))
            if final_type is not None:
                final_type = f"[{repeat_expr}]{final_type}"
            else:
                # repeat-expr が文字列でない、もしくは type が不明な場合は、型不明として None にする
                final_type = None
        # repeat-eosの場合は、型を [..]Type の形式にする
        if d.get("repeat") == "eos":
            if final_type is not None:
                final_type = f"[..]{final_type}"
            else:
                # repeat が eos で type が不明な場合も、型不明として None にする
                final_type = None
        if d.get("size-eos") is not None:
            if final_type is not None:
                final_type = f"[..]{final_type}"
            else:
                # size-eos があるのに type が不明な場合も、型不明として None にする
                final_type = None
        if final_type is None:
            print(
                f"Warning: Could not determine type for attribute '{d.get('id', '_')}' in {d}. Setting type to None."
            )
        return cls(
            id=cast(str, d.get("id", "_")),
            type=final_type,
            if_expr=d.get("if"),
        )


input_dir = "save/kaitai_struct_formats/"
output_dir = "save/bgn_formats/"


def sanitize_name(name: str) -> str:
    brgen_keywords = {"format", "enum", "match", "config"}
    if name in brgen_keywords:
        return f"{name}_"
    return name


def sanitize_and_avoid_conflicts(name: str, existing_names: set[str]) -> str:
    sanitized_name = sanitize_name(name)
    if sanitized_name in existing_names:
        return f"{sanitized_name}_"
    return sanitized_name


def dump_format(
    format_name: str,
    ksy_data: dict,
    base_indent: str,
) -> list[str]:
    nested_types = ksy_data.get("types", {})
    seq = ksy_data.get("seq", [])
    nested_enums = ksy_data.get("enums", {})
    instances = ksy_data.get("instances", {})
    attributes: List[KsyAttribute] = []
    for attr in seq:
        if isinstance(attr, dict):
            attributes.append(KsyAttribute.from_dict(attr))
    lines = [f"format {format_name}:"]

    for attr in attributes:
        if attr.if_expr is not None:
            condition = kaitai_expr_to_bgn_expr(attr.if_expr)
            lines.append(f"  if {condition}:")
            current_lines = len(lines)
        identifier = sanitize_and_avoid_conflicts(
            attr.id, set(nested_types.keys()) | set(nested_enums.keys())
        )
        if isinstance(attr.type, str):
            if identifier in nested_types or identifier in nested_enums:
                identifier += "_"
            if attr.type == "strz":
                lines.append(f"  {identifier}: format:")
                lines.append(f"    body: [..]u8")
                lines.append(f'    :"\\0"')
            else:
                lines.append(f"  {identifier}: {attr.type}")
        elif isinstance(attr.type, KsySwitch):
            lines.append(f'  config.kaitai.switch_on.id("{attr.id}")')
            target = sanitize_and_avoid_conflicts(
                attr.type.target, set(nested_types.keys()) | set(nested_enums.keys())
            )
            lines.append(f"  match {target}:")
            for case_value, case_type in attr.type.cases.items():

                lines.append(
                    f"    {case_value} => {case_type['key']} :{case_type['type']}"
                )
            lines.append("   ")
        else:
            # type が None の場合は、型不明としてコメントアウトして出力
            lines.append(f"  # {identifier}: unknown type")
        if attr.if_expr is not None:
            # if ブロックのインデントを追加
            lines = (
                lines[:current_lines]
                + ["    " + line for line in lines[current_lines:]]
                + lines[current_lines + 1 :]
            )
    for nested_name, nested_data in nested_types.items():
        nested_lines = dump_format(nested_name, nested_data, "  ")
        lines.extend(nested_lines)
    for enum_name, enum_data in nested_enums.items():
        lines.append(f"  enum {enum_name}:")
        for enum_value, member_name in enum_data.items():
            if isinstance(member_name, str):
                lines.append(f"    {sanitize_name(member_name)} = {enum_value}")
            elif isinstance(member_name, dict) and "id" in member_name:
                lines.append(f"    {sanitize_name(member_name['id'])} = {enum_value}")
    for instance_name, instance_data in instances.items():
        lines.append(f"  fn {instance_name}() -> u64:")
        if "value" in instance_data:
            value_expr = kaitai_expr_to_bgn_expr(str(instance_data["value"]))
            lines.append(f"    return {value_expr}")
        else:
            lines.append(f"    .. # instance '{instance_name}' has no value expression")
    if len(lines) == 1:
        # フォーマットに属性がない場合は、空のフォーマットとして出力
        lines.append("  .. # no attributes")
    # indent を追加して返す
    return [base_indent + line for line in lines]


def convert_ksy_to_bgn(ksy_path: str, bgn_path: str) -> None:
    with open(ksy_path, "r", encoding="utf-8") as f:
        ksy_data = yaml.safe_load(f)

    root_path = Path(ksy_path).absolute().as_posix()

    # print(f"Converting {ksy_path} to {bgn_path} (root: {root_path})")

    # ルート構造体の属性を抽出
    # BGN形式で出力
    with open(bgn_path, "w", encoding="utf-8") as f:
        f.write("# This file is auto-generated from Kaitai Struct format\n")
        f.write('config.kaitai.original_file = "file:///{}"\n\n'.format(root_path))
        # currently only output parsed structures
        format_name = os.path.splitext(os.path.basename(ksy_path))[0]
        f.write("\n".join(dump_format(format_name, ksy_data, "")))
        f.write("\n\n")


def main() -> None:
    os.makedirs(output_dir, exist_ok=True)
    for root, _, files in os.walk(input_dir):
        for file in files:
            if file.endswith(".ksy"):
                ksy_path = os.path.join(root, file)
                bgn_path = os.path.join(output_dir, os.path.splitext(file)[0] + ".bgn")
                convert_ksy_to_bgn(ksy_path, bgn_path)


main()
