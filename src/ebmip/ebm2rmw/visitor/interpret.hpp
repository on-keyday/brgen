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
#include "ebmcodegen/stub/util.hpp"
#include "ebmgen/common.hpp"
#include "inst.hpp"
#include "ebmcodegen/stub/ops_macros.hpp"
#include "number/hex/bin2hex.h"

namespace ebm2rmw {
    struct Function {
        ebm::StatementRef id;
        bool has_member = false;
    };

    using TmpCodeWriter = futils::code::CodeWriter<std::string>;

    struct Value {
        std::variant<std::monostate,
                     std::uint64_t,
                     std::string,
                     std::uint8_t*,
                     std::shared_ptr<Value>,
                     std::unordered_map<std::uint64_t, std::shared_ptr<Value>>,
                     Function,
                     std::vector<std::shared_ptr<Value>>>
            value;

        void unref() {
            if (auto ptr = std::get_if<std::shared_ptr<Value>>(&value)) {
                value = ptr->get()->value;
            }
            if (auto elem = std::get_if<std::uint8_t*>(&value)) {
                value = **elem;
            }
        }

        void may_object(InitialContext& ctx) {
            // if monotstate, make it an object
            if (std::holds_alternative<std::monostate>(value)) {
                value = std::unordered_map<std::uint64_t, std::shared_ptr<Value>>{};
                if (ctx.flags().print_state) {
                    futils::wrap::cout_wrap() << "  create object\n";
                }
            }
        }

        void may_bytes(std::size_t size, InitialContext& ctx) {
            if (std::holds_alternative<std::monostate>(value)) {
                std::string buf;
                buf.resize(size);
                value = buf;
                if (ctx.flags().print_state) {
                    futils::wrap::cout_wrap() << "  create bytes of size " << size << "\n";
                }
            }
        }

        void may_vector(InitialContext& ctx) {
            if (std::holds_alternative<std::monostate>(value)) {
                value = std::vector<std::shared_ptr<Value>>{};
                if (ctx.flags().print_state) {
                    futils::wrap::cout_wrap() << "  create vector\n";
                }
            }
        }

        void print(InitialContext& ctx, TmpCodeWriter& w) {
            if (std::holds_alternative<std::uint64_t>(value)) {
                w.write(std::to_string(std::get<std::uint64_t>(value)));
            }
            else if (std::holds_alternative<std::string>(value)) {
                std::string hex_output;
                futils::number::hex::to_hex(hex_output, std::get<std::string>(value));
                w.write("<string> ", hex_output);
            }
            else if (std::holds_alternative<std::shared_ptr<Value>>(value)) {
                w.write("<address> {");
                std::get<std::shared_ptr<Value>>(value)->print(ctx, w);
                w.write("}");
            }
            else if (std::holds_alternative<std::unordered_map<std::uint64_t, std::shared_ptr<Value>>>(value)) {
                w.writeln("{");
                {
                    auto indented = w.indent_scope();
                    auto& m = std::get<std::unordered_map<std::uint64_t, std::shared_ptr<Value>>>(value);
                    bool first = true;
                    for (auto& [k, v] : m) {
                        if (first) {
                            first = false;
                        }
                        else {
                            if (w.out().back() == '\n') {
                                w.out().pop_back();
                                w.should_write_indent(false);
                            }
                            w.writeln(",");
                        }
                        auto ident = ctx.identifier(ebm::StatementRef{k});
                        w.write(ident);
                        w.write(": ");
                        v->print(ctx, w);
                    }
                    if (w.out().back() != '\n') {
                        w.writeln();
                    }
                }
                w.writeln("}");
            }
            else if (std::holds_alternative<Function>(value)) {
                auto ident = ctx.identifier(std::get<Function>(value).id);
                w.write("<function ", ident, ">");
            }
            else if (std::holds_alternative<std::vector<std::shared_ptr<Value>>>(value)) {
                w.writeln("[");
                {
                    auto indented = w.indent_scope();
                    auto& vec = std::get<std::vector<std::shared_ptr<Value>>>(value);
                    for (size_t i = 0; i < vec.size(); i++) {
                        if (i > 0) {
                            if (w.out().back() == '\n') {
                                w.out().pop_back();
                                w.should_write_indent(false);
                            }
                            w.writeln(",");
                        }
                        vec[i]->print(ctx, w);
                    }
                    if (w.out().back() != '\n') {
                        w.writeln();
                    }
                }
                w.write("]");
            }
            else if (std::holds_alternative<std::uint8_t*>(value)) {
                w.write("<bytes>");
            }
            else {
                w.write("<uninitialized>");
            }
        }
    };

    struct StackFrame {
        std::shared_ptr<Value> self;
        std::map<std::uint64_t, std::shared_ptr<Value>> params;
        std::map<std::uint64_t, std::shared_ptr<Value>> heap;
        std::vector<std::pair<ebm::OpCode, Value>> stack;
    };

    inline std::string instruction_args(const ebm::Instruction& instr, InitialContext& ctx, size_t ip) {
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

        ebmgen::expected<std::shared_ptr<Value>> new_object(InitialContext& ctx, ebm::StatementRef ref) {
            auto obj = std::make_shared<Value>();
            std::unordered_map<std::uint64_t, std::shared_ptr<Value>> c{};
            MAYBE(fields, ctx.get(ref));
            MAYBE(field_list, fields.body.struct_decl());
            auto res = handle_fields(ctx, field_list.fields, true, [&](ebm::StatementRef field_ref, const ebm::Statement& field) -> ebmgen::expected<void> {
                MAYBE(field_body, field.body.field_decl());
                if (ctx.is(ebm::TypeKind::STRUCT, field_body.field_type)) {
                    MAYBE(type_impl, ctx.get(field_body.field_type));
                    MAYBE(struct_, type_impl.body.id());
                    MAYBE(nested_obj, new_object(ctx, from_weak(struct_)));
                    c[get_id(field_ref)] = nested_obj;
                }
                else {
                    c[get_id(field_ref)] = std::make_shared<Value>();
                }
                return {};
            });
            if (!res) {
                return ebmgen::unexpect_error(std::move(res.error()));
            }
            if (auto enc = field_list.encode_fn()) {
                c[get_id(*enc)] = std::make_shared<Value>(Function{*enc});
            }
            if (auto dec = field_list.decode_fn()) {
                c[get_id(*dec)] = std::make_shared<Value>(Function{*dec});
            }
            if (auto funcs = field_list.methods()) {
                for (auto& func_ref : funcs->container) {
                    c[get_id(func_ref)] = std::make_shared<Value>(Function{func_ref});
                }
            }
            return obj;
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
        ebmgen::expected<void> interpret(InitialContext& ctx, ebm::StatementRef self_type, std::map<std::uint64_t, std::shared_ptr<Value>>& params) {
            size_t ip = 0;
            bool no_error = false;
            const auto frame = new_frame(no_error);
            auto& this_ = *call_stack.back();
            this_.params = params;
            MAYBE(self_obj, new_object(ctx, self_type));
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
                        self->print(ctx, w);
                        w.writeln();
                        w.writeln("Params:");
                        for (auto& [k, v] : params) {
                            w.write("  ", std::to_string(k), ": ");
                            v->print(ctx, w);
                            w.writeln();
                        }
                        w.writeln("Heap:");
                        for (auto& [k, v] : heap) {
                            w.write("  ", std::to_string(k), ": ");
                            v->print(ctx, w);
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
                auto args = instruction_args(instr.instr, ctx, ip);
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
                            right.unref();
                            left.unref();
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
                            operand.unref();
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
                        // For simplicity, we skip actual byte array creation
                        MAYBE(imm, instr.instr.imm());
                        if (auto v = imm.size()) {
                            std::string buf;
                            buf.resize(v->value());
                            stack_push(Value{std::move(buf)});
                        }
                        else {
                            stack_push(Value{std::string()});
                        }
                        break;
                    }
                    case ebm::OpCode::LOAD_SELF: {
                        stack_push(Value{self});
                        break;
                    }
                    case ebm::OpCode::LOAD_LOCAL: {
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
                            found->second->print(ctx, w);
                            futils::wrap::cout_wrap() << w.out() << "\n";
                        }
                        stack_push(Value{found->second});
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
                        auto val_ptr = std::make_shared<Value>(std::move(val));
                        if (ctx.flags().print_state) {
                            futils::wrap::cout_wrap() << "  heap_store: [" << reg->index.id.value() << "] = ";
                            TmpCodeWriter w;
                            val_ptr->print(ctx, w);
                            futils::wrap::cout_wrap() << w.out() << "\n";
                        }
                        heap[reg->index.id.value()] = val_ptr;
                        break;
                    }
                    case ebm::OpCode::STORE_REF: {
                        if (stack.size() < 2) {
                            return ebmgen::unexpect_error("stack underflow on STORE_REF");
                        }
                        auto ref = stack_pop();
                        auto val = stack_pop();
                        if (!std::holds_alternative<std::shared_ptr<Value>>(ref.value)) {
                            return ebmgen::unexpect_error("STORE_REF target is not a reference");
                        }
                        val.unref();
                        auto ref_ptr = std::get<std::shared_ptr<Value>>(ref.value);
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
                        if (!std::holds_alternative<std::shared_ptr<Value>>(base_val.value)) {
                            return ebmgen::unexpect_error("base value is not an array");
                        }
                        auto base_ptr = std::get<std::shared_ptr<Value>>(base_val.value);
                        if (!std::holds_alternative<std::string>(base_ptr->value)) {
                            return ebmgen::unexpect_error("base value is not an array");
                        }
                        auto& arr = std::get<std::string>(base_ptr->value);
                        if (!std::holds_alternative<std::uint64_t>(index_val.value)) {
                            return ebmgen::unexpect_error("index value is not an integer");
                        }
                        auto index = std::get<std::uint64_t>(index_val.value);
                        if (index >= arr.size()) {
                            return ebmgen::unexpect_error("array index out of bounds");
                        }
                        std::uint8_t* elem = (std::uint8_t*)&arr[index];
                        stack_push(Value{elem});
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
                            size_expr.unref();
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
                        if (!std::holds_alternative<std::shared_ptr<Value>>(target.value)) {
                            return ebmgen::unexpect_error("READ_BYTES target is not an object");
                        }
                        auto& arr = std::get<std::shared_ptr<Value>>(target.value);
                        arr->may_bytes(offset_value + size, ctx);
                        if (!std::holds_alternative<std::string>(arr->value)) {
                            return ebmgen::unexpect_error("READ_BYTES target is not a byte array");
                        }
                        auto& byte_array = std::get<std::string>(arr->value);
                        byte_array.resize(offset_value + size);  // Simulate reading bytes by resizing
                        auto read = input.substr(input_pos, size);
                        if (read.size() < size) {
                            return ebmgen::unexpect_error("expected {} bytes, but only {} bytes available in input", size, read.size());
                        }
                        std::copy(read.begin(), read.end(), byte_array.begin() + offset_value);
                        input_pos += size;
                        stack_push(Value{std::move(arr)});
                        break;
                    }
                    case ebm::OpCode::LOAD_MEMBER: {
                        if (stack.empty()) {
                            return ebmgen::unexpect_error("stack underflow on LOAD_MEMBER");
                        }
                        auto base_val = stack_pop();
                        if (!std::holds_alternative<std::shared_ptr<Value>>(base_val.value)) {
                            return ebmgen::unexpect_error("base value is not an object");
                        }
                        auto base_ptr = std::get<std::shared_ptr<Value>>(base_val.value);
                        base_ptr->may_object(ctx);
                        if (!std::holds_alternative<std::unordered_map<std::uint64_t, std::shared_ptr<Value>>>(base_ptr->value)) {
                            return ebmgen::unexpect_error("base value is not an object");
                        }
                        auto& obj = std::get<std::unordered_map<std::uint64_t, std::shared_ptr<Value>>>(base_ptr->value);
                        auto member_id = instr.instr.member_id();
                        auto& ref = obj[member_id->id.value()];
                        if (!ref) {
                            ref = std::make_shared<Value>();
                            if (ctx.flags().print_state) {
                                futils::wrap::cout_wrap() << "  create member [" << member_id->id.value() << "]\n";
                            }
                            if (ctx.config().env.has_function(*member_id)) {
                                *ref = Value{Function{member_id->id}};
                            }
                            else if (auto prop = ctx.get_field<"property_decl">(*member_id)) {
                                if (ctx.config().env.has_function(prop->getter_function.id)) {
                                    // invoke getter function and store result in ref
                                    auto func_enter = ctx.config().env.new_function(prop->getter_function.id);
                                    bool no_error = false;
                                    auto frame = new_frame(no_error);
                                    auto& new_this = *call_stack.back();
                                    new_this.self = base_ptr;
                                    size_t func_ip = 0;
                                    MAYBE_VOID(r, interpret_impl(ctx, func_ip));
                                    if (stack.empty()) {
                                        return ebmgen::unexpect_error("stack underflow after getter call");
                                    }
                                    *ref = stack_pop();
                                    ref->unref();
                                    no_error = true;
                                }
                            }
                        }
                        stack_push(Value{ref});
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
                        std::map<std::uint64_t, std::shared_ptr<Value>> call_params;
                        for (int i = static_cast<int>(num_args->value()) - 1; i >= 0; i--) {
                            auto arg_val = stack_pop();
                            arg_val.unref();
                            auto arg_ptr = std::make_shared<Value>(std::move(arg_val));
                            auto param_ref = decl.params.container[i];
                            call_params[param_ref.id.value()] = arg_ptr;
                        }
                        auto new_self = stack_pop();
                        if (!std::holds_alternative<std::shared_ptr<Value>>(new_self.value)) {
                            return ebmgen::unexpect_error("CALL new self is not an object");
                        }
                        auto func_enter = ctx.config().env.new_function(func.id);
                        bool no_error = false;
                        const auto frame = new_frame(no_error);
                        auto& new_this = *call_stack.back();
                        new_this.params = std::move(call_params);
                        new_this.self = std::get<std::shared_ptr<Value>>(new_self.value);
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
                        MAYBE(obj, new_object(ctx, *struct_id));
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
                        if (stack.empty()) {
                            return ebmgen::unexpect_error("stack underflow on IS_ERROR");
                        }
                        auto val = stack_pop();
                        val.unref();
                        bool is_error = std::holds_alternative<std::string>(val.value);
                        stack_push(Value{std::uint64_t(is_error ? 1 : 0)});
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
                        if (!std::holds_alternative<std::shared_ptr<Value>>(val.value)) {
                            return ebmgen::unexpect_error("VECTOR_PUSH target is not an object");
                        }
                        auto vec_ptr = std::get<std::shared_ptr<Value>>(val.value);
                        vec_ptr->may_vector(ctx);
                        if (!std::holds_alternative<std::vector<std::shared_ptr<Value>>>(vec_ptr->value)) {
                            return ebmgen::unexpect_error("VECTOR_PUSH target is not a vector");
                        }
                        auto& vec = std::get<std::vector<std::shared_ptr<Value>>>(vec_ptr->value);
                        auto elem_ptr = std::make_shared<Value>(std::move(elem_val));
                        vec.push_back(std::move(elem_ptr));
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