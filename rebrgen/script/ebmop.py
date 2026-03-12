import json
import subprocess as sp

schema_json = json.loads(sp.check_output(["./tool/ebmcodegen", "--mode", "spec-json"]))

enums_map = {e["name"]: e for e in schema_json["enums"]}

binary_ops = enums_map["BinaryOp"]["members"]
unary_ops = enums_map["UnaryOp"]["members"]
op_codes = enums_map["OpCode"]["members"]


def macro_name(kind: str, name: str) -> str:
    return "EBM_OP_" + kind.upper() + "_" + name.upper()


print("// Auto-generated file, do not edit directly.")
print("#pragma once")
print("#include <ebm/extended_binary_module.hpp>")
print("#define APPLY_BINARY_OP(op,...) \\")
first = True
for op in binary_ops:
    if not first:
        print(" \\")
    print(
        "    if(op == ebm::BinaryOp::"
        + op["name"]
        + ") return (__VA_ARGS__)([](auto&& a, auto&& b) { return (a)"
        + str(op["alt_name"])
        + "(b); }); ",
        end="",
    )
    first = False
print()
print("#define APPLY_UNARY_OP(op,...) \\")
first = True
for op in unary_ops:
    if not first:
        print(" \\")
    print(
        "    if(op == ebm::UnaryOp::"
        + op["name"]
        + ") return (__VA_ARGS__)([](auto&& a) { return "
        + str(op["alt_name"])
        + "(a); });",
        end="",
    )
    first = False
print()

print("namespace ebm {")
print("constexpr auto BinaryOp_to_OpCode(BinaryOp op) -> OpCode {")
print("    switch(op) {")
for op in binary_ops:
    print(
        "        case BinaryOp::"
        + op["name"]
        + ": return OpCode::"
        + [x for x in op_codes if x.get("alt_name") == op["name"]][0]["name"]
        + ";"
    )
print("    }")
print("    return OpCode::NOP;")
print("}")
print()
print("constexpr auto OpCode_to_BinaryOp(OpCode op) -> std::optional<BinaryOp> {")
print("    switch(op) {")
for op in binary_ops:
    print(
        "        case OpCode::"
        + [x for x in op_codes if x.get("alt_name") == op["name"]][0]["name"]
        + ": return BinaryOp::"
        + op["name"]
        + ";"
    )
print("        default:")
print("            return std::nullopt;")
print("    }")
print("}")
print()


print("constexpr auto UnaryOp_to_OpCode(UnaryOp op) -> OpCode {")
print("    switch(op) {")
for op in unary_ops:
    print(
        "        case UnaryOp::"
        + op["name"]
        + ": return OpCode::"
        + [x for x in op_codes if x.get("alt_name") == op["name"]][0]["name"]
        + ";"
    )
print("    }")
print("    return OpCode::NOP;")
print("}")
print()
print("constexpr auto OpCode_to_UnaryOp(OpCode op) -> std::optional<UnaryOp> {")
print("    switch(op) {")
for op in unary_ops:
    print(
        "        case OpCode::"
        + [x for x in op_codes if x.get("alt_name") == op["name"]][0]["name"]
        + ": return UnaryOp::"
        + op["name"]
        + ";"
    )
print("        default:")
print("            return std::nullopt;")
print("    }")
print("}")

print("} // namespace ebm")
