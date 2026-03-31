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
#include "ebmcodegen/stub/js_cancel.hpp"

namespace ebm2rmw {
    struct Function {
        ebm::StatementRef id;
        bool has_member = false;
    };

    using TmpCodeWriter = futils::code::CodeWriter<std::string>;
    using VariantEvalFn = std::function<std::optional<size_t>(ebm::TypeRef, ObjectRef)>;
    using BytesArena = std::vector<std::vector<std::uint8_t>>;

    inline ebmgen::expected<std::uint64_t> decode_uint64(ObjectRef obj_ref, size_t strict_size = 0) {
        if (obj_ref.raw_object.size() > 8 || (strict_size && obj_ref.raw_object.size() != strict_size)) [[unlikely]] {
            if (strict_size) {
                return ebmgen::unexpect_error("invalid uint64 object size: {}, expected {}", obj_ref.raw_object.size(), strict_size);
            }
            return ebmgen::unexpect_error("invalid uint64 object size: {}", obj_ref.raw_object.size());
        }
        std::uint64_t int_value = 0;
        for (size_t i = 0; i < obj_ref.raw_object.size(); i++) {
            int_value |= static_cast<std::uint64_t>(static_cast<unsigned char>(obj_ref.raw_object[i])) << (i * 8);
        }
        return int_value;
    }

    inline ebmgen::expected<void> encode_uint64(ObjectRef& obj_ref, std::uint64_t value, size_t strict_size = 0) {
        if (obj_ref.raw_object.size() > 8 || (strict_size && obj_ref.raw_object.size() != strict_size)) [[unlikely]] {
            if (strict_size) {
                return ebmgen::unexpect_error("invalid uint64 object size: {}, expected {}", obj_ref.raw_object.size(), strict_size);
            }
            return ebmgen::unexpect_error("invalid uint64 object size");
        }
        for (size_t i = 0; i < obj_ref.raw_object.size(); i++) {
            obj_ref.raw_object[i] = static_cast<std::uint8_t>((value >> (i * 8)) & 0xFF);
        }
        return {};
    }

    inline void print_layout(InitialContext& ctx, TmpCodeWriter& w, ObjectRef ref, BytesArena* arena = nullptr, const ObjectRef* parent = nullptr, const VariantEvalFn& variant_eval = nullptr) {
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
                        if (type_kind == ebm::TypeKind::STRUCT || type_kind == ebm::TypeKind::STRUCT_UNION) {
                            print_layout(ctx, w, *res, arena, &ref, variant_eval);
                        }
                        else if (type_kind == ebm::TypeKind::VARIANT) {
                            auto variant_desc = ctx.get_field<"variant_desc">(field.type.type);
                        }
                        else if (type_kind == ebm::TypeKind::VECTOR) {
                            if (auto slot = decode_uint64(*res); slot) {
                                w.write("slot=", std::to_string(*slot));
                                if (arena && ctx.flags().print_vector_content && *slot > 0) {
                                    size_t actual_index = *slot - 1;
                                    if (actual_index < arena->size()) {
                                        auto& byte_array = (*arena)[actual_index];
                                        auto elem_layout = access.get_vector_element_type(field.type.type);
                                        if (elem_layout && elem_layout->size > 0 && byte_array.size() % elem_layout->size == 0) {
                                            size_t count = byte_array.size() / elem_layout->size;
                                            w.writeln(" [");
                                            auto inner = w.indent_scope();
                                            for (size_t ei = 0; ei < count; ei++) {
                                                w.write("[", std::to_string(ei), "]: ");
                                                auto elem_raw = futils::view::wvec(byte_array).substr(ei * elem_layout->size, elem_layout->size);
                                                print_layout(ctx, w, ObjectRef{elem_layout->type, elem_raw}, arena, nullptr, variant_eval);
                                                w.writeln();
                                            }
                                            w.write("]");
                                        }
                                        else {
                                            std::string hex_output;
                                            futils::number::hex::to_hex(hex_output, futils::view::rvec(byte_array));
                                            w.write(" [", hex_output, "]");
                                        }
                                    }
                                }
                            }
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
            if (type_kind == ebm::TypeKind::STRUCT_UNION) {
                // Try compile-and-run to determine active variant
                auto* union_layout = access.get_union_layout(ref.type);
                bool displayed = false;
                if (variant_eval && parent && union_layout) {
                    auto variant_idx = variant_eval(ref.type, *parent);
                    if (variant_idx && *variant_idx < union_layout->variants.size()) {
                        auto& vl = union_layout->variants[*variant_idx];
                        auto variant_raw = ref.raw_object.substr(0, vl.size);
                        w.write("<variant ", std::to_string(*variant_idx), "> ");
                        print_layout(ctx, w, ObjectRef{vl.type, variant_raw}, arena, parent, variant_eval);
                        displayed = true;
                    }
                }
                if (!displayed) {
                    w.write("<STRUCT_UNION> ");
                    std::string hex_output;
                    futils::number::hex::to_hex(hex_output, ref.raw_object);
                    w.write(hex_output);
                }
            }
            else {
                if (type_kind) {
                    w.write("<", to_string(*type_kind), "> ");
                }
                std::string hex_output;
                futils::number::hex::to_hex(hex_output, ref.raw_object);
                w.write(hex_output);
            }
        }
        else {
            w.write("<object of unknown type>");
        }
    }

    struct Value {
        std::variant<std::monostate,
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

        void as_int() {
            unref();
            if (auto obj_ref = std::get_if<ObjectRef>(&value)) {
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
        std::vector<Value> params;
        std::vector<Value> locals;
        std::vector<std::pair<ebm::OpCode, Value>> stack;
        std::vector<std::uint8_t> local_bytes;  // for storing local variables of struct/bytes type
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
        std::vector<std::uint8_t> output_buf;

        // Saved after decode for use in encode
        std::vector<std::uint8_t> decoded_self_bytes;
        ebm::TypeRef decoded_self_type;

        ebmgen::expected<ObjectRef> new_object(InitialContext& ctx, futils::view::wvec local_arena, ebm::TypeRef ref, LayoutScratch scratch) {
            auto offset = scratch.offset();
            auto size = scratch.size();
            auto substr = local_arena.substr(offset, size);
            if (substr.size() != size) {
                return ebmgen::unexpect_error("invalid local arena object size: expected {}, got {}", size, substr.size());
            }
            substr.fill(0);  // zero-initialize the memory
            return ObjectRef(ref, substr);
        }

        ebmgen::expected<ObjectRef> vector_alloc_back(InitialContext& ctx, ObjectRef vec, size_t element_size, ebm::TypeRef element_type) {
            MAYBE(index, decode_uint64(vec));
            if (index == 0) {
                bytes_arena.emplace_back();
                index = bytes_arena.size();  // 1-based index
                MAYBE_VOID(_, encode_uint64(vec, index, pointer_size));
            }
            size_t actual_index = index - 1;
            if (actual_index >= bytes_arena.size()) {
                return ebmgen::unexpect_error("invalid vector index: {} (out of {})", actual_index, bytes_arena.size());
            }
            bytes_arena[actual_index].resize(bytes_arena[actual_index].size() + element_size);
            bytes_arena_usage += element_size;
            auto elem_place = futils::view::wvec(bytes_arena[actual_index]).substr(bytes_arena[actual_index].size() - element_size, element_size);
            return ObjectRef(element_type, elem_place);
        }

        ebmgen::expected<futils::view::wvec> get_bytes(ObjectRef& obj_ref, size_t size, ebm::TypeKind type_kind) {
            if (type_kind == ebm::TypeKind::VECTOR) {
                MAYBE(index, decode_uint64(obj_ref));
                if (index == 0) {
                    bytes_arena.emplace_back();
                    index = bytes_arena.size();  // 1-based index
                    MAYBE_VOID(_, encode_uint64(obj_ref, index, pointer_size));
                }
                size_t actual_index = index - 1;
                if (actual_index >= bytes_arena.size()) {
                    return ebmgen::unexpect_error("invalid byte array index: {} (out of {})", actual_index, bytes_arena.size());
                }
                auto& byte_array = bytes_arena[actual_index];
                byte_array.resize(size);
                bytes_arena_usage += size;
                return futils::view::wvec(byte_array);
            }
            else if (type_kind == ebm::TypeKind::ARRAY) {
                if (obj_ref.raw_object.size() < size) {
                    return ebmgen::unexpect_error("invalid array object size: expected {}, got {}", size, obj_ref.raw_object.size());
                }
                return futils::view::wvec(obj_ref.raw_object);
            }
            return {};
        }

       private:
        std::vector<std::shared_ptr<StackFrame>> frame_pool;

        auto new_frame(bool& no_error) {
            if (frame_pool.empty()) {
                frame_pool.push_back(std::make_shared<StackFrame>());
            }
            auto frame = frame_pool.back();
            frame_pool.pop_back();
            call_stack.push_back(frame);
            return futils::helper::defer([&] {
                if (no_error) {
                    auto popped_frame = call_stack.back();
                    call_stack.pop_back();
                    frame_pool.push_back(std::move(popped_frame));
                }
            });
        }

        std::uint64_t stats_op_count[256] = {0};

       public:
        VariantEvalFn make_variant_eval_fn(InitialContext& ctx) {
            return [this, &ctx](ebm::TypeRef type, ObjectRef parent_self) -> std::optional<size_t> {
                auto* layout_ctx = ctx.config().layout_context;
                if (!layout_ctx) return std::nullopt;
                auto* selector_ref = layout_ctx->get_struct_union_selector_fn(type);
                if (!selector_ref) return std::nullopt;
                auto vres = eval_struct_union_variant(ctx, *selector_ref, parent_self);
                if (!vres) return std::nullopt;
                return *vres;
            };
        }

        ebmgen::expected<size_t> eval_struct_union_variant(InitialContext& ctx, ebm::StatementRef selector_fn_ref, ObjectRef parent_self) {
            auto fn_guard = ctx.config().env.new_function(selector_fn_ref);
            bool no_error = false;
            const auto frame = new_frame(no_error);
            auto& this_ = *call_stack.back();
            this_.self = parent_self;
            size_t ip = 0;
            auto res = interpret_impl(ctx, ip);
            if (!res) {
                return ebmgen::unexpect_error(std::move(res.error()));
            }
            no_error = true;
            if (this_.locals.empty()) {
                return ebmgen::unexpect_error("selector function produced no result");
            }
            auto& result_val = this_.locals[0];
            if (!std::holds_alternative<std::uint64_t>(result_val.value)) {
                return ebmgen::unexpect_error("selector function result is not an integer");
            }
            return static_cast<size_t>(std::get<std::uint64_t>(result_val.value));
        }

        ebmgen::expected<void> interpret(InitialContext& ctx, ebm::StatementRef self_type, std::vector<Value>& params) {
            size_t ip = 0;
            bool no_error = false;
            auto start = ebmcodegen::Timepoint{};
            const auto frame = new_frame(no_error);
            auto& this_ = *call_stack.back();
            this_.params = params;
            LayoutAccess access(ctx);
            MAYBE(struct_layout, access.get_struct_layout_detail(self_type));
            std::vector<std::uint8_t> self_bytes(struct_layout.size);
            LayoutScratch layout{};
            layout.size(struct_layout.size);
            MAYBE(self_obj, new_object(ctx, futils::view::wvec(self_bytes), struct_layout.type, layout));
            this_.self = self_obj;
            auto res = interpret_impl(ctx, ip);
            auto end = ebmcodegen::Timepoint{};
            auto variant_eval_fn = make_variant_eval_fn(ctx);
            auto dump_stack = [&] {
                auto& env = ctx.config().env;
                futils::code::CodeWriter<std::string> w;
                size_t call_stack_depth = 0;
                for (auto& frame : call_stack) {
                    auto& self = frame->self;
                    auto& params = frame->params;
                    auto& locals = frame->locals;
                    auto& stack = frame->stack;
                    w.writeln("=== Stack ", std::to_string(call_stack_depth), " ===");
                    w.writeln("IP: ", std::to_string(ip));
                    if (env.get_instructions().size() > ip) {
                        w.writeln("Current Instruction: ", to_string(env.get_instructions()[ip].instr.op, true), " ", env.get_instructions()[ip].str_repr);
                    }
                    w.writeln("Self:");
                    // Do NOT pass variant_eval_fn here: dump_stack iterates call_stack,
                    // and eval_struct_union_variant would push to call_stack causing iterator invalidation.
                    print_layout(ctx, w, self, &bytes_arena, nullptr, nullptr);
                    w.writeln();
                    w.writeln("Params:");
                    for (size_t i = 0; i < params.size(); i++) {
                        w.write("  [", std::to_string(i), "]: ");
                        params[i].print(ctx, w);
                        w.writeln();
                    }
                    w.writeln("Locals:");
                    for (size_t i = 0; i < locals.size(); i++) {
                        w.write("  [", std::to_string(i), "]: ");
                        locals[i].print(ctx, w);
                        w.writeln();
                    }
                    w.writeln("Stack:");
                    for (size_t i = 0; i < stack.size(); i++) {
                        w.write("  [", std::to_string(i), "]: ");
                        stack[i].second.print(ctx, w);
                        w.write(" by ", to_string(stack[i].first, true));
                        w.writeln();
                    }
                    if (call_stack_depth == 0) {
                        w.writeln("Bytes Arena: chunk=", std::to_string(bytes_arena.size()), " usage=", std::to_string(bytes_arena_usage), " bytes");
                    }
                    w.writeln("Local Bytes Arena: size=", std::to_string(frame->local_bytes.size()), " bytes");
                    call_stack_depth++;
                }
                for (size_t op = 0; op < 256; op++) {
                    if (stats_op_count[op] > 0) {
                        w.writeln("Op ", to_string(static_cast<ebm::OpCode>(op), true), ": ", std::to_string(stats_op_count[op]));
                    }
                }
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end.point - start.point);
                w.writeln("Execution time: ", std::to_string(duration.count()), " ms");
                futils::wrap::cout_wrap() << w.out();
            };
            if (!res) {
                if (ctx.flags().print_final_stack) {
                    dump_stack();
                }
                return ebmgen::unexpect_error(std::move(res.error()));
            }
            else {
                no_error = true;
                // save decoded self for encode step
                decoded_self_bytes = std::move(self_bytes);
                decoded_self_type = self_obj.type;
                if (ctx.flags().print_final_stack) {
                    dump_stack();
                    TmpCodeWriter vw;
                    ObjectRef final_self(decoded_self_type, futils::view::wvec(decoded_self_bytes));
                    print_layout(ctx, vw, final_self, &bytes_arena, nullptr, variant_eval_fn);
                    futils::wrap::cout_wrap() << "=== Decoded Self (with variant) ===\n" << vw.out() << "\n";
                }
            }
            return res;
        }

        ebmgen::expected<void> interpret_encode(InitialContext& ctx, std::vector<Value>& params) {
            if (decoded_self_bytes.empty()) {
                return ebmgen::unexpect_error("no decoded self available for encode (decode must run first)");
            }
            output_buf.clear();
            output_buf.reserve(decoded_self_bytes.size());
            size_t ip = 0;
            bool no_error = false;
            auto start = ebmcodegen::Timepoint{};
            const auto frame = new_frame(no_error);
            auto& this_ = *call_stack.back();
            this_.params = params;
            this_.self = ObjectRef(decoded_self_type, futils::view::wvec(decoded_self_bytes));
            auto res = interpret_impl(ctx, ip);
            auto end = ebmcodegen::Timepoint{};
            auto variant_eval_fn = make_variant_eval_fn(ctx);
            auto dump_stack = [&] {
                auto& env = ctx.config().env;
                futils::code::CodeWriter<std::string> w;
                size_t call_stack_depth = 0;
                for (auto& frame : call_stack) {
                    auto& self = frame->self;
                    auto& params = frame->params;
                    auto& locals = frame->locals;
                    auto& stack = frame->stack;
                    w.writeln("=== Stack ", std::to_string(call_stack_depth), " ===");
                    w.writeln("IP: ", std::to_string(ip));
                    if (env.get_instructions().size() > ip) {
                        w.writeln("Current Instruction: ", to_string(env.get_instructions()[ip].instr.op, true), " ", env.get_instructions()[ip].str_repr);
                    }
                    w.writeln("Self:");
                    // Do NOT pass variant_eval_fn here: dump_stack iterates call_stack,
                    // and eval_struct_union_variant would push to call_stack causing iterator invalidation.
                    print_layout(ctx, w, self, &bytes_arena, nullptr, nullptr);
                    w.writeln();
                    w.writeln("Params:");
                    for (size_t i = 0; i < params.size(); i++) {
                        w.write("  [", std::to_string(i), "]: ");
                        params[i].print(ctx, w);
                        w.writeln();
                    }
                    w.writeln("Locals:");
                    for (size_t i = 0; i < locals.size(); i++) {
                        w.write("  [", std::to_string(i), "]: ");
                        locals[i].print(ctx, w);
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
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end.point - start.point);
                w.writeln("Execution time (encode): ", std::to_string(duration.count()), " ms");
                futils::wrap::cout_wrap() << w.out();
            };
            if (!res) {
                if (ctx.flags().print_final_stack) {
                    dump_stack();
                }
                return ebmgen::unexpect_error(std::move(res.error()));
            }
            else {
                no_error = true;
                if (ctx.flags().print_final_stack) {
                    dump_stack();
                }
            }
            return res;
        }

       private:
        BytesArena bytes_arena;
        size_t bytes_arena_usage = 0;
        std::vector<std::shared_ptr<StackFrame>> call_stack;

        ebmgen::expected<void> interpret_impl(InitialContext& ctx, size_t& ip) {
            auto& env = ctx.config().env;
            auto& this_ = *call_stack.back();
            auto& self = this_.self;
            auto& params = this_.params;
            auto& stack = this_.stack;
            auto& local_bytes_arena = this_.local_bytes;
            local_bytes_arena.resize(ctx.config().env.get_current_function()->structs_area);
            stack.reserve(128);  // arbitrary initial stack size, can grow as needed
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
            auto& locals = this_.locals;
            locals.resize(env.get_current_function()->local_count);
            bool no_error = false;
            auto print_step = [&](const Instruction& instr) {
                auto args = instruction_args(instr.instr, ctx, ip, env.get_current_function());
                futils::wrap::cout_wrap() << "ip=" << ip << ", op=" << to_string(instr.instr.op, true);
                if (args.size() > 0) {
                    futils::wrap::cout_wrap() << ", args={" << args << "}";
                }
                futils::wrap::cout_wrap() << ", str_repr=" << instr.str_repr << ", stack_size=" << stack.size() << ", call_stack_size=" << call_stack.size() << "\n";
            };
            js_may_cancel_task();
            auto instructions = env.get_instructions().data();
            size_t instruction_count = env.get_instructions().size();
            while (ip < instruction_count) {
                auto& instr = instructions[ip];
                stats_op_count[static_cast<std::uint8_t>(instr.instr.op)]++;
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
                            right.as_int();
                            left.as_int();
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
                            operand.as_int();
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
                            auto value = futils::view::wvec(local_bytes_arena).substr(instr.scratch, v->value());
                            if (value.size() != v->value()) {
                                return ebmgen::unexpect_error("invalid local bytes size: expected {}, got {}", v->value(), value.size());
                            }
                            stack_push(Value{ObjectRef(instr.type_info, value)});
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
                        auto& found = locals[instr.scratch];
                        if (ctx.flags().print_state) {
                            futils::wrap::cout_wrap() << "  local_load: [" << instr.scratch << "] = ";
                            TmpCodeWriter w;
                            found.print(ctx, w);
                            futils::wrap::cout_wrap() << w.out() << "\n";
                        }
                        if (instr.instr.op == ebm::OpCode::LOAD_LOCAL) {
                            stack_push(Value{found});
                        }
                        else {
                            stack_push(Value{&found});
                        }
                        break;
                    }
                    case ebm::OpCode::STORE_LOCAL_IMM: {
                        MAYBE(imm, instr.instr.value());
                        if (ctx.flags().print_state) {
                            futils::wrap::cout_wrap() << "  local_store_imm: [" << instr.scratch << "] = " << imm.value() << "\n";
                        }
                        locals[instr.scratch] = Value{imm.value()};
                        break;
                    }
                    case ebm::OpCode::EQ_IMM: {
                        if (stack.empty()) {
                            return ebmgen::unexpect_error("stack underflow on EQ_IMM");
                        }
                        auto val = stack_pop();
                        val.as_int();
                        if (!std::holds_alternative<std::uint64_t>(val.value)) {
                            return ebmgen::unexpect_error("EQ_IMM operand is not an integer");
                        }
                        std::uint64_t result = (std::get<std::uint64_t>(val.value) == instr.instr.value()->value()) ? 1 : 0;
                        stack_push(Value{result});
                        break;
                    }
                    case ebm::OpCode::STORE_LOCAL: {
                        if (stack.empty()) {
                            return ebmgen::unexpect_error("stack underflow on STORE_LOCAL");
                        }
                        auto val = stack_pop();
                        val.unref();
                        if (ctx.flags().print_state) {
                            futils::wrap::cout_wrap() << "  locals_store: [" << instr.scratch << "] = ";
                            TmpCodeWriter w;
                            val.print(ctx, w);
                            futils::wrap::cout_wrap() << w.out() << "\n";
                        }
                        locals[instr.scratch] = std::move(val);
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
                                    for (size_t i = 0; i < object_ref->raw_object.size(); i++) {
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
                    case ebm::OpCode::ARRAY_GET_IMM: {
                        if (stack.empty()) {
                            return ebmgen::unexpect_error("stack underflow on ARRAY_GET_IMM");
                        }
                        auto base_val = stack_pop();
                        base_val.unref();
                        if (!std::holds_alternative<ObjectRef>(base_val.value)) {
                            return ebmgen::unexpect_error("base value is not an array");
                        }
                        auto base_ptr = std::get<ObjectRef>(base_val.value);
                        LayoutScratch ls{instr.scratch};
                        auto element_size = ls.offset() > 0 ? ls.offset() : size_t(1);
                        auto base_kind = ebm::TypeKind(ls.size());
                        auto index = instr.instr.index()->value();
                        if (base_kind == ebm::TypeKind::VECTOR) {
                            MAYBE(arena_index, decode_uint64(base_ptr));
                            if (arena_index == 0) {
                                return ebmgen::unexpect_error("ARRAY_GET_IMM on uninitialized vector");
                            }
                            size_t actual_index = arena_index - 1;
                            if (actual_index >= bytes_arena.size()) {
                                return ebmgen::unexpect_error("ARRAY_GET_IMM: invalid vector index: {} (out of {})", actual_index, bytes_arena.size());
                            }
                            auto& byte_array = bytes_arena[actual_index];
                            auto raw = futils::view::wvec(byte_array).substr(index * element_size, element_size);
                            if (raw.size() != element_size) {
                                return ebmgen::unexpect_error("ARRAY_GET_IMM: vector element out of bounds: index={}, element_size={}, array_size={}", index, element_size, byte_array.size());
                            }
                            stack_push(Value{ObjectRef{instr.type_info, raw}});
                        }
                        else {
                            auto element = base_ptr.raw_object.substr(index * element_size, element_size);
                            if (element.size() != element_size) {
                                return ebmgen::unexpect_error("ARRAY_GET_IMM index out of bounds: index={}, element_size={}, array_size={}", index, element_size, base_ptr.raw_object.size());
                            }
                            stack_push(Value{ObjectRef{instr.type_info, element}});
                        }
                        break;
                    }
                    case ebm::OpCode::ARRAY_GET: {
                        if (stack.size() < 2) {
                            return ebmgen::unexpect_error("stack underflow on ARRAY_GET");
                        }
                        auto index_val = stack_pop();
                        index_val.as_int();
                        auto base_val = stack_pop();
                        base_val.unref();
                        if (!std::holds_alternative<ObjectRef>(base_val.value)) {
                            return ebmgen::unexpect_error("base value is not an array");
                        }
                        auto base_ptr = std::get<ObjectRef>(base_val.value);
                        if (!std::holds_alternative<std::uint64_t>(index_val.value)) {
                            return ebmgen::unexpect_error("index value is not an integer");
                        }
                        LayoutScratch ls{instr.scratch};
                        auto element_size = ls.offset() > 0 ? ls.offset() : size_t(1);
                        auto base_kind = ebm::TypeKind(ls.size());
                        auto index = std::get<std::uint64_t>(index_val.value);
                        if (base_kind == ebm::TypeKind::VECTOR) {
                            MAYBE(arena_index, decode_uint64(base_ptr));
                            if (arena_index == 0) {
                                return ebmgen::unexpect_error("ARRAY_GET on uninitialized vector");
                            }
                            size_t actual_index = arena_index - 1;
                            if (actual_index >= bytes_arena.size()) {
                                return ebmgen::unexpect_error("ARRAY_GET: invalid vector index: {} (out of {})", actual_index, bytes_arena.size());
                            }
                            auto& byte_array = bytes_arena[actual_index];
                            auto raw = futils::view::wvec(byte_array).substr(index * element_size, element_size);
                            if (raw.size() != element_size) {
                                return ebmgen::unexpect_error("ARRAY_GET: vector element out of bounds: index={}, element_size={}, array_size={}", index, element_size, byte_array.size());
                            }
                            stack_push(Value{ObjectRef{instr.type_info, raw}});
                        }
                        else {
                            auto element = base_ptr.raw_object.substr(index * element_size, element_size);
                            if (element.size() != element_size) {
                                return ebmgen::unexpect_error("ARRAY_GET index out of bounds: index={}, element_size={}, array_size={}", index, element_size, base_ptr.raw_object.size());
                            }
                            stack_push(Value{ObjectRef{instr.type_info, element}});
                        }
                        break;
                    }
                    case ebm::OpCode::READ_BYTE: {
                        if (stack.empty()) [[unlikely]] {
                            return ebmgen::unexpect_error("stack underflow on READ_BYTE");
                        }
                        MAYBE(offset, instr.instr.offset());
                        auto target = stack_pop();
                        target.unref();
                        if (!std::holds_alternative<ObjectRef>(target.value)) [[unlikely]] {
                            return ebmgen::unexpect_error("READ_BYTE target is not an object");
                        }
                        auto& arr = std::get<ObjectRef>(target.value);
                        if (arr.raw_object.size() < offset.value() + 1) [[unlikely]] {
                            return ebmgen::unexpect_error("READ_BYTE target array is too small");
                        }
                        arr.raw_object[offset.value()] = input[input_pos];
                        input_pos += 1;
                        break;
                    }
                    case ebm::OpCode::READ_BYTES: {
                        MAYBE(imm, instr.instr.imm());
                        std::size_t size = 0;
                        if (auto static_size = imm.size()) {
                            size = static_size->value();
                        }
                        else {
                            if (stack.empty()) [[unlikely]] {
                                return ebmgen::unexpect_error("stack underflow on READ_BYTES");
                            }
                            auto size_expr = stack_pop();
                            size_expr.as_int();
                            if (!std::holds_alternative<std::uint64_t>(size_expr.value)) [[unlikely]] {
                                return ebmgen::unexpect_error("READ_BYTES size is not an integer");
                            }
                            size = static_cast<std::size_t>(std::get<std::uint64_t>(size_expr.value));
                        }
                        if (stack.empty()) [[unlikely]] {
                            return ebmgen::unexpect_error("stack underflow on READ_BYTES target");
                        }
                        size_t offset_value = 0;
                        auto offset = instr.instr.offset();
                        if (offset) {
                            offset_value = offset->value();
                        }
                        auto target = stack_pop();
                        auto kind = ebm::TypeKind(instr.scratch);
                        target.unref();
                        if (!std::holds_alternative<ObjectRef>(target.value)) [[unlikely]] {
                            return ebmgen::unexpect_error("READ_BYTES target is not an object");
                        }
                        auto read = input.substr(input_pos, size);
                        if (read.size() < size) [[unlikely]] {
                            return ebmgen::unexpect_error("expected {} bytes, but only {} bytes available in input", size, read.size());
                        }
                        auto& arr = std::get<ObjectRef>(target.value);
                        // currently, vector allocation point should be front of the call stack,
                        // so that we can assume the vector elements are alive until interpret() returns.
                        MAYBE(byte_array, get_bytes(arr, offset_value + size, kind));
                        if (offset_value + size > byte_array.size()) [[unlikely]] {
                            return ebmgen::unexpect_error("READ_BYTES out of bounds: offset {} + size {} exceeds array size {}", offset_value, size, byte_array.size());
                        }
                        std::copy(read.begin(), read.end(), byte_array.begin() + offset_value);
                        input_pos += size;
                        break;
                    }
                    case ebm::OpCode::LOAD_SELF_MEMBER: {
                        LayoutScratch scratch{instr.scratch};
                        auto range = self.raw_object.substr(scratch.offset(), scratch.size());
                        if (range.size() != scratch.size()) [[unlikely]] {
                            return ebmgen::unexpect_error("LOAD_SELF_MEMBER range out of bounds: {} expect {} but got {} (offset: {})", instr.str_repr, scratch.size(), range.size(), scratch.offset());
                        }
                        stack_push(Value{ObjectRef(instr.type_info, range)});
                        break;
                    }
                    case ebm::OpCode::LOAD_MEMBER: {
                        if (stack.empty()) [[unlikely]] {
                            return ebmgen::unexpect_error("stack underflow on LOAD_MEMBER");
                        }
                        auto member_base_val = stack_pop();
                        member_base_val.unref();
                        if (!std::holds_alternative<ObjectRef>(member_base_val.value)) [[unlikely]] {
                            return ebmgen::unexpect_error("base value is not an object");
                        }
                        auto base_ref = std::get<ObjectRef>(member_base_val.value);
                        LayoutScratch scratch{instr.scratch};
                        auto range = base_ref.raw_object.substr(scratch.offset(), scratch.size());
                        if (range.size() != scratch.size()) [[unlikely]] {
                            return ebmgen::unexpect_error("LOAD_MEMBER range out of bounds");
                        }
                        stack_push(Value{ObjectRef(instr.type_info, range)});
                        break;
                    }
                    case ebm::OpCode::CALL_GETTER: {
                        if (stack.empty()) {
                            return ebmgen::unexpect_error("stack underflow on CALL_GETTER");
                        }
                        auto obj_val = stack_pop();
                        obj_val.unref();
                        if (!std::holds_alternative<ObjectRef>(obj_val.value)) {
                            return ebmgen::unexpect_error("CALL_GETTER target is not an object");
                        }
                        auto func_enter = ctx.config().env.new_function(*instr.instr.func_id());
                        bool no_error = false;
                        const auto frame = new_frame(no_error);
                        auto& new_this = *call_stack.back();
                        new_this.self = std::get<ObjectRef>(obj_val.value);
                        size_t func_ip = 0;
                        MAYBE_VOID(r, interpret_impl(ctx, func_ip));
                        no_error = true;
                        break;
                    }
                    case ebm::OpCode::LOAD_PARAM: {
                        stack_push(Value{params[instr.scratch]});
                        break;
                    }
                    case ebm::OpCode::LOAD_FUNC: {
                        auto func_id = instr.instr.func_id();
                        if (!func_id) {
                            return ebmgen::unexpect_error("missing function id in LOAD_FUNC");
                        }
                        stack_push(Value{Function{*func_id}});
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
                        if (!std::holds_alternative<Function>(callee.value)) [[unlikely]] {
                            return ebmgen::unexpect_error("CALL target is not a function");
                        }
                        auto func = std::get<Function>(callee.value);
                        std::vector<Value> call_params;
                        for (int i = static_cast<int>(num_args->value()) - 1; i >= 0; i--) {
                            auto arg_val = stack_pop();
                            arg_val.unref();
                            call_params.push_back(std::move(arg_val));
                        }
                        auto new_self = stack_pop();
                        new_self.unref();
                        if (!std::holds_alternative<ObjectRef>(new_self.value)) [[unlikely]] {
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
                    case ebm::OpCode::CALL_DIRECT: {
                        auto num_args = instr.instr.arg_num();
                        if (!num_args) [[unlikely]] {
                            return ebmgen::unexpect_error("missing argument number in CALL_DIRECT");
                        }
                        if (stack.size() < num_args->value() + 1) [[unlikely]] {
                            return ebmgen::unexpect_error("stack underflow on CALL_DIRECT");
                        }
                        auto func = *instr.instr.func_id();
                        std::vector<Value> call_params;
                        for (int i = static_cast<int>(num_args->value()) - 1; i >= 0; i--) {
                            auto arg_val = stack_pop();
                            arg_val.unref();
                            call_params.push_back(std::move(arg_val));
                        }
                        auto new_self = stack_pop();
                        new_self.unref();
                        if (!std::holds_alternative<ObjectRef>(new_self.value)) [[unlikely]] {
                            return ebmgen::unexpect_error("CALL_DIRECT new self is not an object");
                        }
                        auto func_enter = ctx.config().env.new_function(func);
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
                        MAYBE(obj, new_object(ctx, local_bytes_arena, instr.type_info, LayoutScratch{instr.scratch}));
                        stack_push(Value{obj});
                        break;
                    }
                    case ebm::OpCode::RET: {
                        auto has_value = instr.instr.ret_value()->has_value();
                        if (has_value) {
                            if (stack.empty()) {
                                return ebmgen::unexpect_error("stack underflow on RET with value");
                            }
                            auto ret_val = stack_pop();
                            if (call_stack.size() > 1) {
                                stack_push_with_stack(call_stack[call_stack.size() - 2]->stack, std::move(ret_val));
                            }
                        }
                        no_error = true;
                        return {};
                    }
                    case ebm::OpCode::POP: {
                        if (stack.empty()) {
                            return ebmgen::unexpect_error("stack underflow on POP");
                        }
                        stack_pop();
                        break;
                    }
                    case ebm::OpCode::WRITE_U8: {
                        if (stack.empty()) [[unlikely]] {
                            return ebmgen::unexpect_error("stack underflow on WRITE_U8");
                        }
                        auto target = stack_pop();
                        target.unref();
                        if (std::holds_alternative<ObjectRef>(target.value)) {
                            auto& obj = std::get<ObjectRef>(target.value);
                            if (obj.raw_object.empty()) [[unlikely]] {
                                return ebmgen::unexpect_error("WRITE_U8 target is empty");
                            }
                            output_buf.push_back(static_cast<std::uint8_t>(obj.raw_object[0]));
                        }
                        else if (std::holds_alternative<std::uint64_t>(target.value)) {
                            output_buf.push_back(static_cast<std::uint8_t>(std::get<std::uint64_t>(target.value)));
                        }
                        else {
                            return ebmgen::unexpect_error("WRITE_U8 target is not an object or integer");
                        }
                        break;
                    }
                    case ebm::OpCode::WRITE_BYTES: {
                        // scratch: lower 32 bits = type_kind, upper 32 bits = static_size (0 = dynamic)
                        LayoutScratch ls{instr.scratch};
                        auto kind = ebm::TypeKind(ls.offset());
                        size_t static_size = ls.size();
                        if (static_size == 0) {
                            // dynamic size: pop size from stack
                            if (stack.size() < 2) [[unlikely]] {
                                return ebmgen::unexpect_error("stack underflow on WRITE_BYTES (need size and target)");
                            }
                            auto size_val = stack_pop();
                            size_val.as_int();
                            if (!std::holds_alternative<std::uint64_t>(size_val.value)) [[unlikely]] {
                                return ebmgen::unexpect_error("WRITE_BYTES dynamic size is not an integer");
                            }
                            // Note: size_val is popped to balance the stack; actual write size
                            // comes from the object's stored data for VECTOR, or raw_object for ARRAY.
                        }
                        if (stack.empty()) [[unlikely]] {
                            return ebmgen::unexpect_error("stack underflow on WRITE_BYTES target");
                        }
                        auto target = stack_pop();
                        target.unref();
                        if (!std::holds_alternative<ObjectRef>(target.value)) [[unlikely]] {
                            return ebmgen::unexpect_error("WRITE_BYTES target is not an object");
                        }
                        auto& arr = std::get<ObjectRef>(target.value);
                        if (kind == ebm::TypeKind::VECTOR) {
                            MAYBE(index, decode_uint64(arr));
                            if (index == 0) {
                                return ebmgen::unexpect_error("WRITE_BYTES vector is uninitialized");
                            }
                            size_t actual_index = index - 1;
                            if (actual_index >= bytes_arena.size()) [[unlikely]] {
                                return ebmgen::unexpect_error("WRITE_BYTES invalid vector index: {} (out of {})", actual_index, bytes_arena.size());
                            }
                            auto& byte_array = bytes_arena[actual_index];
                            output_buf.insert(output_buf.end(), byte_array.begin(), byte_array.end());
                        }
                        else {
                            size_t bytes_to_write = std::min(static_size > 0 ? static_size : arr.raw_object.size(), arr.raw_object.size());
                            output_buf.insert(output_buf.end(), arr.raw_object.begin(), arr.raw_object.begin() + bytes_to_write);
                        }
                        break;
                    }
                    case ebm::OpCode::VECTOR_PUSH: {
                        if (stack.size() < 2) {
                            return ebmgen::unexpect_error("stack underflow on VECTOR_PUSH");
                        }
                        auto elem_val = stack_pop();
                        elem_val.unref();
                        auto val = stack_pop();
                        val.unref();
                        if (!std::holds_alternative<ObjectRef>(val.value)) {
                            return ebmgen::unexpect_error("VECTOR_PUSH target is not an object");
                        }
                        auto vec_ptr = std::get<ObjectRef>(val.value);
                        MAYBE(elem_place, vector_alloc_back(ctx, vec_ptr, instr.scratch, instr.type_info));
                        if (std::holds_alternative<std::uint64_t>(elem_val.value)) {
                            auto int_val = std::get<std::uint64_t>(elem_val.value);
                            MAYBE_VOID(_, encode_uint64(elem_place, int_val, false));
                        }
                        else if (std::holds_alternative<ObjectRef>(elem_val.value)) {
                            auto obj_ref = std::get<ObjectRef>(elem_val.value);
                            if (obj_ref.raw_object.size() != elem_place.raw_object.size()) {
                                return ebmgen::unexpect_error("element size mismatch in VECTOR_PUSH: expected {}, got {}", elem_place.raw_object.size(), obj_ref.raw_object.size());
                            }
                            std::copy(obj_ref.raw_object.begin(), obj_ref.raw_object.end(), elem_place.raw_object.begin());
                        }
                        else {
                            return ebmgen::unexpect_error("unsupported element type in VECTOR_PUSH");
                        }
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