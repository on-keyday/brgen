/*license*/
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include "../codegen.hpp"
#include "code/code_writer.h"
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/common.hpp"
#include "inst.hpp"
#include "ebmcodegen/stub/ops_macros.hpp"
#include "layout.hpp"
#include "number/hex/bin2hex.h"

namespace ebm2rmw {
    struct Function {
        ebm::StatementRef id;
        bool has_member = false;
    };

    using TmpCodeWriter = futils::code::CodeWriter<std::string>;

    inline void print_layout(InitialContext& ctx, TmpCodeWriter& w, ObjectRef ref) {
        LayoutAccess access(ctx);
        if (auto detail = access.get_struct_layout_detail(ref.type)) {
            w.writeln("<object of type ", ctx.identifier(detail->struct_ref), "> {");
            {
                auto indented = w.indent_scope();
                w.writeln("size: ", std::to_string(detail->size));
                for (auto& field : detail->fields) {
                    auto field_name = ctx.identifier(field.field);
                    w.write(field_name, ": ");
                    if (auto res = access.read_member(ref, field.field); res) {
                        auto type_kind = ctx.get_kind(field.type.type);
                        if (type_kind) {
                            w.write("<", to_string(*type_kind), "> ");
                        }
                        w.write("size=", std::to_string(res->raw_object.size()), " ");
                        if (type_kind == ebm::TypeKind::STRUCT) {
                            print_layout(ctx, w, *res);
                        }
                        else {
                            std::string hex_output;
                            futils::number::hex::to_hex(hex_output, res->raw_object);
                            w.write(hex_output);
                        }
                    }
                    else {
                        w.write("<error reading field>");
                    }
                    w.writeln();
                }
            }
            w.write("}");
        }
        else if (auto normal_type = access.get_type_layout(ref.type)) {
            auto type_kind = ctx.get_kind(normal_type->type);
            if (type_kind) {
                w.write("<", to_string(*type_kind), "> ");
            }
            std::string hex_output;
            futils::number::hex::to_hex(hex_output, ref.raw_object);
            w.write(hex_output);
        }
        else {
            w.write("<object of unknown type>");
        }
    }

    struct Value {
        std::variant<std::monostate,
                     /*
                     std::string,
                      std::uint8_t*,
                      std::shared_ptr<Value>,
                      std::unordered_map<std::uint64_t, std::shared_ptr<Value>>,
                      std::vector<std::shared_ptr<Value>>,
                      */
                     // ^^^ old heap, to keep existing references valid. will be removed later.
                     ObjectRef,
                     Value*,
                     Function,
                     std::uint64_t>
            value;

        void unref() {
            if (auto elem = std::get_if<Value*>(&value)) {
                value = (**elem).value;
            }
        }

        void as_int(InitialContext& ctx) {
            unref();
            if (auto obj_ref = std::get_if<ObjectRef>(&value)) {
                if (ctx.get_kind(obj_ref->type) != ebm::TypeKind::UINT) {
                    return;
                }
                std::uint64_t int_value = 0;
                for (size_t i = 0; i < obj_ref->raw_object.size(); i++) {
                    int_value |= static_cast<std::uint64_t>(static_cast<unsigned char>(obj_ref->raw_object[i])) << (i * 8);
                }
                value = int_value;
            }
        }

        void print(InitialContext& ctx, TmpCodeWriter& w) {
            if (std::holds_alternative<std::uint64_t>(value)) {
                w.write(std::to_string(std::get<std::uint64_t>(value)));
            }
            else if (std::holds_alternative<Function>(value)) {
                auto ident = ctx.identifier(std::get<Function>(value).id);
                w.write("<function ", ident, ">");
            }
            else if (std::holds_alternative<ObjectRef>(value)) {
                auto& obj_ref = std::get<ObjectRef>(value);
                w.write("<object ref> ");
                print_layout(ctx, w, obj_ref);
            }
            else if (std::holds_alternative<Value*>(value)) {
                w.write("<value ref> {");
                std::get<Value*>(value)->print(ctx, w);
                w.write("}");
            }
            else {
                w.write("<uninitialized>");
            }
        }
    };

    struct StackFrame {
        ObjectRef self;
        std::map<std::uint64_t, Value> params;
        std::map<std::uint64_t, Value> heap;
        std::vector<std::pair<ebm::OpCode, Value>> stack;

        std::vector<std::vector<std::uint8_t>> byte_arrays;
    };

    inline std::string instruction_args(const ebm::Instruction& instr, InitialContext& ctx, size_t ip, const FunctionDecl* func_decl) {
        std::string args;
        instr.visit([&](auto&& self, std::string_view name, auto&& value) -> void {
            if (name == "op") {
                return;
            }
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_pointer_v<T>) {
                if (value) {
                    self(self, name, *value);
                }
            }
            else {
                if (args.size() > 0) {
                    if (args.back() != '{') {
                        args += ", ";
                    }
                }
                if constexpr (std::is_same_v<T, ebm::StatementRef>) {
                    auto ident = ctx.identifier(value);
                    args += std::format("{}={}({})", name, ident, get_id(value));
                }
                else if constexpr (std::is_same_v<T, ebm::Varint>) {
                    args += std::format("{}={}", name, value.value());
                }
                else if constexpr (std::is_same_v<T, bool>) {
                    args += std::format("{}={}", name, value ? "true" : "false");
                }
                else if constexpr (std::is_integral_v<T>) {
                    args += std::format("{}={}", name, value);
                }
                else if constexpr (has_visit<T, decltype(self)>) {
                    args += std::format("{}={{", name);
                    value.visit(self);
                    if constexpr (std::is_same_v<T, ebm::JumpOffset>) {
                        if (value.backward()) {
                            args += std::format(",ip-{} = {}", value.offset.value(), ip - value.offset.value());
                        }
                        else {
                            args += std::format(",ip+{} = {}", value.offset.value(), ip + value.offset.value());
                        }
                    }
                    if constexpr (std::is_same_v<T, ebm::RegisterIndex>) {
                        if (instr.op == ebm::OpCode::LOAD_LOCAL || instr.op == ebm::OpCode::STORE_LOCAL ||
                            instr.op == ebm::OpCode::LOAD_LOCAL_REF) {
                            auto found = func_decl->local_indices.find(value.index);
                            if (found != func_decl->local_indices.end()) {
                                args += std::format(", local {}", found->second);
                            }
                            else {
                                args += ", unknown local";
                            }
                        }
                    }
                    args += "}";
                }
                else {
                    args += std::format("{}={}", name, to_string(value));
                }
            }
        });
        return args;
    }

    struct RuntimeEnv {
        futils::view::rvec input;
        size_t input_pos = 0;

        ebmgen::expected<ObjectRef> new_object(InitialContext& ctx, ebm::StatementRef ref, StackFrame& frame) {
            LayoutAccess access{ctx};
            MAYBE(layout, access.get_struct_layout(ref));
            auto& bytes_arena = frame.byte_arrays;
            bytes_arena.emplace_back();
            auto& raw_object = bytes_arena.back();
            raw_object.resize(layout.size);
            return ObjectRef(layout.type, futils::view::wvec(raw_object));
        }

       private:
        auto new_frame(bool& no_error) {
            call_stack.emplace_back(std::make_shared<StackFrame>());
            return futils::helper::defer([&] {
                if (no_error) {
                    call_stack.pop_back();
                }
            });
        }

       public:
        ebmgen::expected<void> interpret(InitialContext& ctx, ebm::StatementRef self_type, std::map<std::uint64_t, Value>& params) {
            size_t ip = 0;
            bool no_error = false;
            const auto frame = new_frame(no_error);
            auto& this_ = *call_stack.back();
            this_.params = params;
            MAYBE(self_obj, new_object(ctx, self_type, this_));
            this_.self = self_obj;
            auto res = interpret_impl(ctx, ip);
            if (!res) {
                auto dump_on_error = [&] {
                    if (!ctx.flags().print_state && !ctx.flags().print_step && !ctx.flags().print_error_stack) {
                        return;
                    }
                    if (no_error) {
                        return;
                    }
                    auto& env = ctx.config().env;
                    futils::code::CodeWriter<std::string> w;
                    size_t call_stack_depth = 0;
                    for (auto& frame : call_stack) {
                        auto& self = frame->self;
                        auto& params = frame->params;
                        auto& heap = frame->heap;
                        auto& stack = frame->stack;
                        w.writeln("=== Stack ", std::to_string(call_stack_depth), " ===");
                        w.writeln("IP: ", std::to_string(ip));
                        if (env.get_instructions().size() > ip) {
                            w.writeln("Current Instruction: ", to_string(env.get_instructions()[ip].instr.op, true), " ", env.get_instructions()[ip].str_repr);
                        }
                        w.writeln("Self:");
                        print_layout(ctx, w, self);
                        w.writeln();
                        w.writeln("Params:");
                        for (auto& [k, v] : params) {
                            w.write("  ", std::to_string(k), ": ");
                            v.print(ctx, w);
                            w.writeln();
                        }
                        w.writeln("Heap:");
                        for (auto& [k, v] : heap) {
                            w.write("  ", std::to_string(k), ": ");
                            v.print(ctx, w);
                            w.writeln();
                        }
                        w.writeln("Stack:");
                        for (size_t i = 0; i < stack.size(); i++) {
                            w.write("  [", std::to_string(i), "]: ");
                            stack[i].second.print(ctx, w);
                            w.write(" by ", to_string(stack[i].first, true));
                            w.writeln();
                        }
                        call_stack_depth++;
                    }
                    futils::wrap::cout_wrap() << w.out();
                };
                dump_on_error();
                return ebmgen::unexpect_error(std::move(res.error()));
            }
            else {
                no_error = true;
            }
            return res;
        }

       private:
        std::vector<std::shared_ptr<StackFrame>> call_stack;

        ebmgen::expected<void> interpret_impl(InitialContext& ctx, size_t& ip) {
            auto& env = ctx.config().env;
            auto& this_ = *call_stack.back();
            auto& self = this_.self;
            auto& params = this_.params;
            auto& stack = this_.stack;
            auto& bytes_arena = this_.byte_arrays;
            auto stack_pop = [&] {
                assert(!stack.empty());
                auto val = std::move(stack.back());
                stack.pop_back();
                if (ctx.flags().print_state) {
                    futils::wrap::cout_wrap() << "  stack_pop: ";
                    TmpCodeWriter w;
                    val.second.print(ctx, w);
                    futils::wrap::cout_wrap() << w.out() << " from opcode " << to_string(val.first, true) << "\n";
                }
                return val.second;
            };
            auto stack_push_with_stack = [&](std::vector<std::pair<ebm::OpCode, Value>>& s, Value val) {
                if (ctx.flags().print_state) {
                    futils::wrap::cout_wrap() << "  stack_push: ";
                    TmpCodeWriter w;
                    val.print(ctx, w);
                    futils::wrap::cout_wrap() << w.out() << " from opcode " << to_string(env.get_instructions()[ip].instr.op, true) << "\n";
                }
                s.push_back(std::make_pair(env.get_instructions()[ip].instr.op, std::move(val)));
            };
            auto stack_push = [&](Value val) {
                stack_push_with_stack(stack, std::move(val));
            };
            auto& heap = this_.heap;
            bool no_error = false;
            auto print_step = [&](const Instruction& instr) {
                auto args = instruction_args(instr.instr, ctx, ip, env.get_current_function());
                futils::wrap::cout_wrap() << "ip=" << ip << ", op=" << to_string(instr.instr.op, true);
                if (args.size() > 0) {
                    futils::wrap::cout_wrap() << ", args={" << args << "}";
                }
                futils::wrap::cout_wrap() << ", str_repr=" << instr.str_repr << ", stack_size=" << stack.size() << ", call_stack_size=" << call_stack.size() << "\n";
            };

            while (ip < env.get_instructions().size()) {
                auto& instr = env.get_instructions()[ip];
                if (ctx.flags().print_step) {
                    print_step(instr);
                }
                if (auto bop = OpCode_to_BinaryOp(instr.instr.op)) {
                    if (stack.size() < 2) {
                        return ebmgen::unexpect_error("stack underflow on binary operation");
                    }
                    auto right = stack_pop();
                    auto left = stack_pop();
                    auto r = [&]() -> ebmgen::expected<void> {
                        APPLY_BINARY_OP(*bop, [&](auto&& op) -> ebmgen::expected<void> {
                            right.as_int(ctx);
                            left.as_int(ctx);
                            if (!std::holds_alternative<std::uint64_t>(left.value) || !std::holds_alternative<std::uint64_t>(right.value)) {
                                return ebmgen::unexpect_error("binary operation operands must be integers");
                            }
                            auto lval = std::get<std::uint64_t>(left.value);
                            auto rval = std::get<std::uint64_t>(right.value);
                            auto result = op(lval, rval);
                            stack_push(Value{result});
                            return ebmgen::expected<void>{};
                        });
                        return ebmgen::unexpect_error("unsupported binary operation in interpreter: {}", to_string(*bop));
                    }();
                    MAYBE_VOID(r_, r);
                    ip++;
                    continue;
                }
                if (auto uop = OpCode_to_UnaryOp(instr.instr.op)) {
                    if (stack.empty()) {
                        return ebmgen::unexpect_error("stack underflow on unary operation");
                    }
                    auto operand = stack_pop();
                    auto r = [&]() -> ebmgen::expected<void> {
                        APPLY_UNARY_OP(*uop, [&](auto&& op) -> ebmgen::expected<void> {
                            operand.as_int(ctx);
                            if (!std::holds_alternative<std::uint64_t>(operand.value)) {
                                return ebmgen::unexpect_error("unary operation operand must be an integer");
                            }
                            auto val = std::get<std::uint64_t>(operand.value);
                            auto result = op(val);
                            stack_push(Value{result});
                            return ebmgen::expected<void>{};
                        });
                        return ebmgen::unexpect_error("unsupported unary operation in interpreter: {}", to_string(*uop));
                    }();
                    MAYBE_VOID(r_, r);
                    ip++;
                    continue;
                }
                switch (instr.instr.op) {
                    case ebm::OpCode::NOP:
                        break;
                    case ebm::OpCode::HALT:
                        return ebmgen::unexpect_error("HALT reached");
                    case ebm::OpCode::PUSH_IMM_INT: {
                        auto val = instr.instr.value();
                        stack_push(Value{val->value()});
                        break;
                    }
                    case ebm::OpCode::NEW_BYTES: {
                        MAYBE(imm, instr.instr.imm());
                        if (auto v = imm.size()) {
                            LayoutAccess access(ctx);
                            auto array_layout = access.get_u8_n_array(v->value());
                            if (!array_layout) {
                                return ebmgen::unexpect_error("failed to get u8 array layout for size {}", v->value());
                            }
                            std::vector<std::uint8_t> byte_array(v->value());
                            bytes_arena.push_back(std::move(byte_array));
                            stack_push(Value{ObjectRef(array_layout->type, bytes_arena.back())});
                        }
                        else {
                            return ebmgen::unexpect_error("missing size immediate in NEW_BYTES");
                        }
                        break;
                    }
                    case ebm::OpCode::LOAD_SELF: {
                        stack_push(Value{self});
                        break;
                    }
                    case ebm::OpCode::LOAD_LOCAL:
                    case ebm::OpCode::LOAD_LOCAL_REF: {
                        auto reg = instr.instr.reg();
                        if (!reg) {
                            return ebmgen::unexpect_error("missing register index in LOAD_LOCAL");
                        }
                        auto found = heap.find(reg->index.id.value());
                        if (found == heap.end()) {
                            return ebmgen::unexpect_error("undefined local variable");
                        }
                        if (ctx.flags().print_state) {
                            futils::wrap::cout_wrap() << "  heap_load: [" << reg->index.id.value() << "] = ";
                            TmpCodeWriter w;
                            found->second.print(ctx, w);
                            futils::wrap::cout_wrap() << w.out() << "\n";
                        }
                        if (instr.instr.op == ebm::OpCode::LOAD_LOCAL) {
                            stack_push(Value{found->second});
                        }
                        else {
                            stack_push(Value{&found->second});
                        }
                        break;
                    }
                    case ebm::OpCode::STORE_LOCAL: {
                        auto reg = instr.instr.reg();
                        if (!reg) {
                            return ebmgen::unexpect_error("missing register index in STORE_LOCAL");
                        }
                        if (stack.empty()) {
                            return ebmgen::unexpect_error("stack underflow on STORE_LOCAL");
                        }
                        auto val = stack_pop();
                        val.unref();
                        if (ctx.flags().print_state) {
                            futils::wrap::cout_wrap() << "  heap_store: [" << reg->index.id.value() << "] = ";
                            TmpCodeWriter w;
                            val.print(ctx, w);
                            futils::wrap::cout_wrap() << w.out() << "\n";
                        }
                        heap[reg->index.id.value()] = std::move(val);
                        break;
                    }
                    case ebm::OpCode::STORE_REF: {
                        if (stack.size() < 2) {
                            return ebmgen::unexpect_error("stack underflow on STORE_REF");
                        }
                        auto ref = stack_pop();
                        auto val = stack_pop();
                        if (!std::holds_alternative<Value*>(ref.value)) {
                            if (auto object_ref = std::get_if<ObjectRef>(&ref.value)) {
                                if (auto objref_src = std::get_if<ObjectRef>(&val.value)) {
                                    if (object_ref->raw_object.size() != objref_src->raw_object.size()) {
                                        return ebmgen::unexpect_error("object size mismatch in STORE_REF: {} vs {}", object_ref->raw_object.size(), objref_src->raw_object.size());
                                    }
                                    std::copy(objref_src->raw_object.begin(), objref_src->raw_object.end(), object_ref->raw_object.begin());
                                    break;
                                }
                                if (auto int_src = std::get_if<std::uint64_t>(&val.value)) {
                                    if (ctx.get_kind(object_ref->type) != ebm::TypeKind::UINT) {
                                        return ebmgen::unexpect_error("target object is not an integer in STORE_REF");
                                    }
                                    for (size_t i = 0; i < sizeof(std::uint64_t); i++) {
                                        object_ref->raw_object[i] = static_cast<char>((*int_src >> (i * 8)) & 0xFF);
                                    }
                                    break;
                                }
                            }
                            return ebmgen::unexpect_error("STORE_REF target is not a reference");
                        }
                        val.unref();
                        auto ref_ptr = std::get<Value*>(ref.value);
                        *ref_ptr = std::move(val);
                        break;
                    }
                    case ebm::OpCode::ASSERT: {
                        if (stack.empty()) {
                            return ebmgen::unexpect_error("stack underflow on ASSERT");
                        }
                        auto cond = stack_pop();
                        cond.unref();
                        if (!std::holds_alternative<std::uint64_t>(cond.value)) {
                            return ebmgen::unexpect_error("ASSERT condition is not an integer");
                        }
                        auto cond_val = std::get<std::uint64_t>(cond.value);
                        if (cond_val == 0) {
                            return ebmgen::unexpect_error("ASSERT failed");
                        }
                        break;
                    }
                    case ebm::OpCode::ARRAY_GET: {
                        if (stack.size() < 2) {
                            return ebmgen::unexpect_error("stack underflow on ARRAY_GET");
                        }
                        auto index_val = stack_pop();
                        auto base_val = stack_pop();
                        base_val.unref();
                        if (!std::holds_alternative<ObjectRef>(base_val.value)) {
                            return ebmgen::unexpect_error("base value is not an array");
                        }
                        auto base_ptr = std::get<ObjectRef>(base_val.value);
                        if (!std::holds_alternative<std::uint64_t>(index_val.value)) {
                            return ebmgen::unexpect_error("index value is not an integer");
                        }
                        auto index = std::get<std::uint64_t>(index_val.value);
                        if (index >= base_ptr.raw_object.size()) {
                            return ebmgen::unexpect_error("array index out of bounds");
                        }
                        auto element = base_ptr.raw_object.substr(index, 1);
                        LayoutAccess access(ctx);
                        MAYBE(element_layout, access.get_array_element_type(base_ptr.type));
                        if (element_layout.size != element.size()) {
                            return ebmgen::unexpect_error("only byte arrays are supported in ARRAY_GET");
                        }
                        stack_push(Value{ObjectRef{element_layout.type, element}});
                        break;
                    }
                    case ebm::OpCode::READ_BYTES: {
                        MAYBE(imm, instr.instr.imm());
                        std::size_t size = 0;
                        if (auto static_size = imm.size()) {
                            size = static_size->value();
                        }
                        else {
                            if (stack.empty()) {
                                return ebmgen::unexpect_error("stack underflow on READ_BYTES");
                            }
                            auto size_expr = stack_pop();
                            size_expr.as_int(ctx);
                            if (!std::holds_alternative<std::uint64_t>(size_expr.value)) {
                                return ebmgen::unexpect_error("READ_BYTES size is not an integer");
                            }
                            size = static_cast<std::size_t>(std::get<std::uint64_t>(size_expr.value));
                        }
                        if (stack.empty()) {
                            return ebmgen::unexpect_error("stack underflow on READ_BYTES target");
                        }
                        size_t offset_value = 0;
                        auto offset = instr.instr.offset();
                        if (offset) {
                            offset_value = offset->value();
                        }
                        auto target = stack_pop();
                        target.unref();
                        if (!std::holds_alternative<ObjectRef>(target.value)) {
                            return ebmgen::unexpect_error("READ_BYTES target is not an object");
                        }
                        auto& arr = std::get<ObjectRef>(target.value);
                        auto& byte_array = arr.raw_object;
                        if (offset_value + size > byte_array.size()) {
                            return ebmgen::unexpect_error("READ_BYTES out of bounds: offset {} + size {} exceeds array size {}", offset_value, size, byte_array.size());
                        }
                        auto read = input.substr(input_pos, size);
                        if (read.size() < size) {
                            return ebmgen::unexpect_error("expected {} bytes, but only {} bytes available in input", size, read.size());
                        }
                        std::copy(read.begin(), read.end(), byte_array.begin() + offset_value);
                        input_pos += size;
                        stack_push(Value{std::move(arr)});
                        break;
                    }
                    case ebm::OpCode::LOAD_MEMBER:
                    case ebm::OpCode::LOAD_MEMBER_REF: {
                        if (stack.empty()) {
                            return ebmgen::unexpect_error("stack underflow on LOAD_MEMBER");
                        }
                        auto member_base_val = stack_pop();
                        member_base_val.unref();
                        if (!std::holds_alternative<ObjectRef>(member_base_val.value)) {
                            return ebmgen::unexpect_error("base value is not an object");
                        }
                        auto base_ref = std::get<ObjectRef>(member_base_val.value);
                        auto member_id = instr.instr.member_id();
                        if (ctx.config().env.has_function(*member_id)) {
                            stack_push(Value{Function{*member_id, true}});
                            break;  // if it's a method, we push a Function wrapper and handle it in CALL
                        }
                        else if (auto prop = ctx.get_field<"property_decl">(*member_id)) {
                            if (ctx.config().env.has_function(prop->getter_function.id)) {
                                // invoke getter function and store result in ref
                                auto func_enter = ctx.config().env.new_function(prop->getter_function.id);
                                bool no_error = false;
                                auto frame = new_frame(no_error);
                                auto& new_this = *call_stack.back();
                                new_this.self = base_ref;
                                size_t func_ip = 0;
                                MAYBE_VOID(r, interpret_impl(ctx, func_ip));
                                if (stack.empty()) {
                                    return ebmgen::unexpect_error("stack underflow after getter call");
                                }
                                no_error = true;
                                break;  // no need to set ref since getter should have pushed the value onto the stack
                            }
                        }
                        LayoutAccess access{ctx};
                        MAYBE(object, access.read_member(base_ref, *member_id));
                        stack_push(Value{object});
                        break;
                    }
                    case ebm::OpCode::LOAD_PARAM: {
                        auto reg = instr.instr.reg();
                        if (!reg) {
                            return ebmgen::unexpect_error("missing register index in LOAD_PARAM");
                        }
                        auto found = params.find(reg->index.id.value());
                        if (found == params.end()) {
                            return ebmgen::unexpect_error("undefined parameter variable");
                        }
                        stack_push(Value{found->second});
                        break;
                    }
                    case ebm::OpCode::CALL: {
                        auto num_args = instr.instr.arg_num();
                        if (!num_args) {
                            return ebmgen::unexpect_error("missing argument number in CALL");
                        }
                        if (stack.size() < num_args->value() + 2) {
                            return ebmgen::unexpect_error("stack underflow on CALL");
                        }
                        auto callee = stack_pop();
                        callee.unref();
                        if (!std::holds_alternative<Function>(callee.value)) {
                            return ebmgen::unexpect_error("CALL target is not a function");
                        }
                        auto func = std::get<Function>(callee.value);
                        MAYBE(func_def, ctx.get(func.id));
                        MAYBE(decl, func_def.body.func_decl());
                        std::map<std::uint64_t, Value> call_params;
                        for (int i = static_cast<int>(num_args->value()) - 1; i >= 0; i--) {
                            auto arg_val = stack_pop();
                            arg_val.unref();
                            auto param_ref = decl.params.container[i];
                            call_params[param_ref.id.value()] = std::move(arg_val);
                        }
                        auto new_self = stack_pop();
                        new_self.unref();
                        if (!std::holds_alternative<ObjectRef>(new_self.value)) {
                            return ebmgen::unexpect_error("CALL new self is not an object");
                        }
                        auto func_enter = ctx.config().env.new_function(func.id);
                        bool no_error = false;
                        const auto frame = new_frame(no_error);
                        auto& new_this = *call_stack.back();
                        new_this.params = std::move(call_params);
                        new_this.self = std::get<ObjectRef>(new_self.value);
                        size_t func_ip = 0;
                        MAYBE_VOID(r, interpret_impl(ctx, func_ip));
                        no_error = true;
                        break;
                    }
                    case ebm::OpCode::JUMP_IF_FALSE: {
                        if (stack.empty()) {
                            return ebmgen::unexpect_error("stack underflow on JUMP_IF_FALSE");
                        }
                        auto cond = stack_pop();
                        cond.unref();
                        if (!std::holds_alternative<std::uint64_t>(cond.value)) {
                            return ebmgen::unexpect_error("JUMP_IF_FALSE condition is not an integer");
                        }
                        auto cond_val = std::get<std::uint64_t>(cond.value);
                        if (cond_val == 0) {
                            MAYBE(offset, instr.instr.target());
                            if (offset.backward()) {
                                ip -= offset.offset.value();
                            }
                            else {
                                ip += offset.offset.value();
                            }
                            continue;
                        }
                        break;
                    }
                    case ebm::OpCode::JUMP: {
                        MAYBE(offset, instr.instr.target());
                        if (offset.backward()) {
                            ip -= offset.offset.value();
                        }
                        else {
                            ip += offset.offset.value();
                        }
                        continue;
                    }
                    case ebm::OpCode::NEW_STRUCT: {
                        auto struct_id = instr.instr.struct_id();
                        if (!struct_id) {
                            return ebmgen::unexpect_error("missing struct id in NEW_STRUCT");
                        }
                        MAYBE(obj, new_object(ctx, *struct_id, this_));
                        stack_push(Value{obj});
                        break;
                    }
                    case ebm::OpCode::PUSH_SUCCESS: {
                        stack_push(Value{std::uint64_t(1)});
                        break;
                    }
                    case ebm::OpCode::RET: {
                        if (!stack.empty()) {
                            auto ret_val = stack_pop();
                            if (call_stack.size() > 1) {
                                stack_push_with_stack(call_stack[call_stack.size() - 2]->stack, std::move(ret_val));
                            }
                        }
                        no_error = true;
                        return {};
                    }
                    case ebm::OpCode::IS_ERROR: {
                        // currently, halt on error, so IS_ERROR always returns false.
                        // In the future, if we want to support error values, we can change this to check if the top of the stack is an error value.
                        if (stack.empty()) {
                            return ebmgen::unexpect_error("stack underflow on IS_ERROR");
                        }
                        auto val = stack_pop();
                        val.unref();
                        stack_push(Value{std::uint64_t(0)});
                        break;
                    }
                    case ebm::OpCode::VECTOR_PUSH: {
                        if (stack.empty()) {
                            return ebmgen::unexpect_error("stack underflow on VECTOR_PUSH");
                        }
                        auto elem_val = stack_pop();
                        elem_val.unref();
                        if (stack.empty()) {
                            return ebmgen::unexpect_error("stack underflow on VECTOR_PUSH");
                        }
                        auto val = stack_pop();
                        val.unref();
                        if (!std::holds_alternative<ObjectRef>(val.value)) {
                            return ebmgen::unexpect_error("VECTOR_PUSH target is not an object");
                        }
                        auto vec_ptr = std::get<ObjectRef>(val.value);
                        break;
                    }
                    default:
                        return ebmgen::unexpect_error("unsupported opcode in interpreter: {}(0x{:x})", to_string(instr.instr.op, true), static_cast<std::uint32_t>(instr.instr.op));
                }
                ip++;
            }
            no_error = true;
            return {};
        }
    };

}  // namespace ebm2rmw