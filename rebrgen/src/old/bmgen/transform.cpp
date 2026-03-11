/*license*/
#include "convert.hpp"
#include <view/span.h>
#include "helper.hpp"
#include "internal.hpp"
#include "transform.hpp"

namespace rebgn {
    void rebind_ident_index(Module& mod) {
        mod.ident_index_table.clear();
        for (auto& i : mod.code) {
            if (auto ident = i.ident(); ident) {
                mod.ident_index_table[ident->value()] = &i - mod.code.data();
            }
        }
    }

    Error remap_programs(Module& m) {
        m.programs.clear();
        for (auto& range : m.ident_to_ranges) {
            auto index = range.range.start.value();
            if (m.code[index].op == AbstractOp::DEFINE_PROGRAM) {
                m.programs.push_back(range.range);
            }
        }
        return none;
    }

    Error add_ident_ranges(Module& m) {
        for (size_t i = 0; i < m.code.size(); i++) {
            AbstractOp end_op;
            switch (m.code[i].op) {
                case AbstractOp::DEFINE_FUNCTION:
                    end_op = AbstractOp::END_FUNCTION;
                    break;
                case AbstractOp::DEFINE_ENUM:
                    end_op = AbstractOp::END_ENUM;
                    break;
                case AbstractOp::DEFINE_FORMAT:
                    end_op = AbstractOp::END_FORMAT;
                    break;
                case AbstractOp::DEFINE_STATE:
                    end_op = AbstractOp::END_STATE;
                    break;
                case AbstractOp::DEFINE_UNION:
                    end_op = AbstractOp::END_UNION;
                    break;
                case AbstractOp::DEFINE_UNION_MEMBER:
                    end_op = AbstractOp::END_UNION_MEMBER;
                    break;
                case AbstractOp::DEFINE_PROGRAM:
                    end_op = AbstractOp::END_PROGRAM;
                    break;
                case AbstractOp::DEFINE_BIT_FIELD:
                    end_op = AbstractOp::END_BIT_FIELD;
                    break;
                case AbstractOp::DEFINE_PROPERTY:
                    end_op = AbstractOp::END_PROPERTY;
                    break;
                case AbstractOp::DEFINE_FALLBACK:
                    end_op = AbstractOp::END_FALLBACK;
                    break;
                default:
                    continue;
            }
            auto ident = m.code[i].ident().value();
            auto start = i;
            for (i = i + 1; i < m.code.size(); i++) {
                if (m.code[i].op == end_op) {
                    break;
                }
            }
            IdentRange range = {.ident = ident, .range = {.start = *varint(start), .end = *varint(i + 1)}};
            m.ident_to_ranges.push_back(std::move(range));
        }
        return none;
    }

    struct FunctionStack {
        size_t current_function_index = 0;
        std::optional<size_t> decoder_parameter_index;
        std::optional<size_t> encoder_parameter_index;
        size_t inner_subrange = 0;

        bool is_decoder() const {
            return decoder_parameter_index.has_value() && !inner_subrange;
        }

        bool is_encoder() const {
            return encoder_parameter_index.has_value() && !inner_subrange;
        }
    };

    void analyze_encoder_decoder_traits(Module& m) {
        std::unordered_map<ObjectID, ObjectID> function_to_coder;
        std::vector<FunctionStack> stack;
        auto common_process = [&](size_t i) {
            if (m.code[i].op == AbstractOp::DEFINE_FUNCTION) {
                stack.push_back(FunctionStack{.current_function_index = i, .inner_subrange = 0});
            }
            if (m.code[i].op == AbstractOp::DECODER_PARAMETER) {
                stack.back().decoder_parameter_index = i;
            }
            if (m.code[i].op == AbstractOp::ENCODER_PARAMETER) {
                stack.back().encoder_parameter_index = i;
            }
            if (m.code[i].op == AbstractOp::END_FUNCTION) {
                stack.pop_back();
            }
            if (m.code[i].op == AbstractOp::BEGIN_DECODE_SUB_RANGE ||
                m.code[i].op == AbstractOp::BEGIN_ENCODE_SUB_RANGE) {
                stack.back().inner_subrange++;
            }
            if (m.code[i].op == AbstractOp::END_DECODE_SUB_RANGE ||
                m.code[i].op == AbstractOp::END_ENCODE_SUB_RANGE) {
                stack.back().inner_subrange--;
            }
        };
        for (size_t i = 0; i < m.code.size(); i++) {
            common_process(i);
            if (stack.empty()) {
                continue;
            }
            auto set_decode_flag = [&](auto&& set) {
                auto idx = stack.back().decoder_parameter_index.value();
                auto flags = m.code[idx].decode_flags().value();
                set(flags);
                m.code[idx].decode_flags(flags);
            };
            auto set_encode_flag = [&](auto&& set) {
                auto idx = stack.back().encoder_parameter_index.value();
                auto flags = m.code[idx].encode_flags().value();
                set(flags);
                m.code[idx].encode_flags(flags);
            };
            if (stack.back().is_decoder()) {
                function_to_coder[m.code[stack.back().current_function_index].ident().value().value()] = *stack.back().decoder_parameter_index;
                if (m.code[i].op == AbstractOp::CAN_READ) {
                    set_decode_flag([](auto& flags) { flags.has_eof(true); });
                }
                if (m.code[i].op == AbstractOp::REMAIN_BYTES) {
                    set_decode_flag([](auto& flags) { flags.has_remain_bytes(true); });
                }
                if (m.code[i].op == AbstractOp::PEEK_INT_VECTOR) {
                    set_decode_flag([](auto& flags) { flags.has_peek(true); });
                }
                if (m.code[i].op == AbstractOp::BACKWARD_INPUT || m.code[i].op == AbstractOp::INPUT_BYTE_OFFSET) {
                    set_decode_flag([](auto& flags) { flags.has_seek(true); });
                }
            }
            else if (stack.back().decoder_parameter_index.has_value()) {
                set_decode_flag([](auto& flags) { flags.has_sub_range(true); });  // sub range will not be propagated by call
            }
            if (stack.back().is_encoder()) {
                function_to_coder[m.code[stack.back().current_function_index].ident().value().value()] = *stack.back().encoder_parameter_index;
                if (m.code[i].op == AbstractOp::BACKWARD_OUTPUT || m.code[i].op == AbstractOp::OUTPUT_BYTE_OFFSET) {
                    set_encode_flag([](auto& flags) { flags.has_seek(true); });
                }
            }
            else if (stack.back().encoder_parameter_index.has_value()) {
                set_encode_flag([](auto& flags) { flags.has_sub_range(true); });
            }
        }
        std::unordered_set<ObjectID> reached;
        bool has_diff = false;
        auto recursive_process = [&](auto&& f, size_t start) -> void {
            if (reached.contains(start)) {
                return;
            }
            reached.insert(start);
            for (size_t i = start; i < m.code.size(); i++) {
                common_process(i);
                if (m.code[i].op == AbstractOp::END_FUNCTION) {
                    return;
                }
                if (stack.back().is_decoder()) {
                    if (m.code[i].op == AbstractOp::CALL_DECODE) {
                        auto found = function_to_coder.find(m.code[i].left_ref().value().value());
                        if (found != function_to_coder.end()) {
                            f(f, m.ident_index_table[found->first]);
                            // copy traits
                            auto idx = stack.back().decoder_parameter_index.value();
                            auto dst = m.code[idx].decode_flags().value();
                            auto src = m.code[found->second].decode_flags().value();
                            auto prev_diff = has_diff;
                            has_diff = has_diff ||
                                       !dst.has_eof() && src.has_eof() ||
                                       !dst.has_remain_bytes() && src.has_remain_bytes() ||
                                       !dst.has_peek() && src.has_peek() ||
                                       !dst.has_seek() && src.has_seek();
                            dst.has_eof(dst.has_eof() || src.has_eof());
                            dst.has_remain_bytes(dst.has_remain_bytes() || src.has_remain_bytes());
                            dst.has_peek(dst.has_peek() || src.has_peek());
                            dst.has_seek(dst.has_seek() || src.has_seek());
                            m.code[idx].decode_flags(dst);
                        }
                    }
                }
                if (stack.back().is_encoder()) {
                    if (m.code[i].op == AbstractOp::CALL_ENCODE) {
                        auto found = function_to_coder.find(m.code[i].left_ref().value().value());
                        if (found != function_to_coder.end()) {
                            f(f, m.ident_index_table[found->first]);
                            // copy traits
                            auto idx = stack.back().encoder_parameter_index.value();
                            auto dst = m.code[idx].encode_flags().value();
                            auto src = m.code[found->second].encode_flags().value();
                            auto prev_diff = has_diff;
                            has_diff = has_diff || !dst.has_seek() && src.has_seek();
                            dst.has_seek(dst.has_seek() || src.has_seek());
                            m.code[idx].encode_flags(dst);
                        }
                    }
                }
            }
        };
        for (size_t i = 0; i < m.code.size(); i++) {
            if (m.code[i].op == AbstractOp::DEFINE_FUNCTION) {
                recursive_process(recursive_process, i);
            }
        }
        while (has_diff) {  // TODO(on-keyday): optimize
            has_diff = false;
            reached.clear();
            for (size_t i = 0; i < m.code.size(); i++) {
                if (m.code[i].op == AbstractOp::DEFINE_FUNCTION) {
                    recursive_process(recursive_process, i);
                }
            }
        }
    }

    Error optimize_type_usage(Module& m) {
        std::unordered_map<ObjectID, std::uint64_t> type_usage;
        std::unordered_set<ObjectID> reached;
        for (auto& c : m.code) {
            if (auto s = c.type()) {
                if (s->ref.value() == 0) {  // null, skip
                    continue;
                }
                reached.insert(s.value().ref.value());
                type_usage[s.value().ref.value()]++;
            }
            if (auto f = c.from_type()) {
                if (f->ref.value() == 0) {  // null, skip
                    continue;
                }
                reached.insert(f.value().ref.value());
                type_usage[f.value().ref.value()]++;
            }
        }
        std::vector<std::tuple<ObjectID /*original*/, std::uint64_t /*usage*/, ObjectID /*map to*/>> sorted;
        for (auto& [k, v] : type_usage) {
            sorted.emplace_back(k, v, 0);
        }
        std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return std::get<1>(a) > std::get<1>(b); });
        if (reached.size() != sorted.size()) {
            return error("Invalid type usage");
        }
        size_t i = 0;
        for (auto& r : reached) {
            std::get<2>(sorted[i]) = r;
            i++;
        }
        std::unordered_map<ObjectID, ObjectID> mapping;
        for (auto& [k, _, v] : sorted) {
            mapping[k] = v;
        }
        std::unordered_map<ObjectID, std::string> new_storage_key_table_rev;
        std::unordered_map<std::string, ObjectID> new_storage_key_table;
        for (auto& mp : mapping) {
            auto old = m.storage_key_table_rev.find(mp.first);
            if (old == m.storage_key_table_rev.end()) {
                return error("Invalid storage key");
            }
            new_storage_key_table_rev[mp.second] = old->second;
            new_storage_key_table[old->second] = mp.second;
        }
        for (auto& c : m.code) {
            if (auto s = c.type()) {
                if (s->ref.value() == 0) {
                    continue;
                }
                c.type(StorageRef{.ref = *varint(mapping[s.value().ref.value()])});
            }
            if (auto f = c.from_type()) {
                if (f->ref.value() == 0) {
                    continue;
                }
                c.from_type(StorageRef{.ref = *varint(mapping[f.value().ref.value()])});
            }
        }
        m.storage_key_table = std::move(new_storage_key_table);
        m.storage_key_table_rev = std::move(new_storage_key_table_rev);
        return none;
    }

    Error sort_immediate(Module& m) {
        std::vector<Code> new_code;
        for (auto& code : m.code) {
            if (code.op == rebgn::AbstractOp::IMMEDIATE_INT || code.op == rebgn::AbstractOp::IMMEDIATE_INT64) {
                new_code.push_back(std::move(code));
            }
        }
        for (auto& code : m.code) {
            if (code.op != rebgn::AbstractOp::IMMEDIATE_INT && code.op != rebgn::AbstractOp::IMMEDIATE_INT64) {
                new_code.push_back(std::move(code));
            }
        }
        m.code = std::move(new_code);
        return none;
    }

#define APPLY_AND_REBIND(f, ...)                    \
    {                                               \
        auto err = f(m __VA_OPT__(, ) __VA_ARGS__); \
        if (err) {                                  \
            return err;                             \
        }                                           \
        rebind_ident_index(m);                      \
    }

    Error transform(Module& m, const std::shared_ptr<ast::Node>& node) {
        APPLY_AND_REBIND(flatten);
        APPLY_AND_REBIND(decide_bit_field_size);
        APPLY_AND_REBIND(bind_encoder_and_decoder);
        APPLY_AND_REBIND(sort_formats, node);
        {
            replace_call_encode_decode_ref(m);
            rebind_ident_index(m);
        }
        APPLY_AND_REBIND(merge_conditional_field);
        APPLY_AND_REBIND(derive_property_functions);
        APPLY_AND_REBIND(derive_bit_field_accessor_functions);
        analyze_encoder_decoder_traits(m);
        APPLY_AND_REBIND(expand_bit_operation);
        APPLY_AND_REBIND(apply_encode_fallback);
        APPLY_AND_REBIND(apply_dynamic_endian_fallback);
        APPLY_AND_REBIND(sort_func_in_format);
        APPLY_AND_REBIND(sort_immediate);
        APPLY_AND_REBIND(generate_cfg1);
        auto err = add_ident_ranges(m);
        if (err) {
            return err;
        }
        remap_programs(m);
        err = optimize_type_usage(m);
        if (err) {
            return err;
        }
        return none;
    }
}  // namespace rebgn
