/*license*/
#pragma once

#include "traverse.h"
#include <number/array.h>
#include <strutil/append.h>

namespace brgen::ast {
    namespace internal {
        template <bool ast_mode, class P1>
        auto print_ptr(const char* ptr_annot, const char* prefix = "", const char* suffix = "") {
            if constexpr (ast_mode) {
                futils::number::Array<char, 50, true> buf{};
                futils::strutil::appends(buf, prefix, "uintptr", suffix);
                return buf;
            }
            else if constexpr (std::is_base_of_v<Node, P1>) {
                futils::number::Array<char, 50, true> buf{};
                futils::strutil::appends(buf, prefix, ptr_annot, "<");
                if constexpr (std::is_same_v<Node, P1>) {
                    futils::strutil::append(buf, "node");
                }
                else {
                    futils::strutil::append(buf, node_type_to_string(P1::node_type_tag));
                }
                futils::strutil::appends(buf, ">", suffix);
                return buf;
            }
            else if constexpr (std::is_same_v<Scope, P1>) {
                futils::number::Array<char, 50, true> buf{};
                futils::strutil::appends(buf, prefix, ptr_annot, "<scope>", suffix);
                return buf;
            }
        }

        template <bool ast_mode, class P1>
        auto print_array() {
            if constexpr (futils::helper::is_template<P1>) {
                using P2 = typename futils::helper::template_of_t<P1>::template param_at<0>;
                if constexpr (futils::helper::is_template_instance_of<P1, std::shared_ptr>) {
                    auto p = print_ptr<ast_mode, P2>("shared_ptr", "array<", ">");
                    return p;
                }
                else if constexpr (futils::helper::is_template_instance_of<P1, std::weak_ptr>) {
                    auto p = print_ptr<ast_mode, P2>("weak_ptr", "array<", ">");
                    return p;
                }
            }
            else {
                static_assert(std::is_same_v<P1, NodeType>);
            }
        }

        auto unwrap(auto& v) {
            if constexpr (std::is_same_v<decltype(v), const char*&>) {
                return v;
            }
            else {
                return v.c_str();
            }
        }

        void dump_base_type(auto&& field, NodeType type) {
            const char* vec[7]{};
            size_t i = 0;
            auto t = int(type);
            auto is = [&](auto v) { return t != int(v) && (t & int(v)) == int(v); };

            if (is(NodeType::literal)) {
                vec[i++] = "literal";
            }
            if (is(NodeType::member)) {
                vec[i++] = "member";
            }
            if (is(NodeType::builtin_member)) {
                vec[i++] = "builtin_member";
            }
            if (is(NodeType::expr)) {
                vec[i++] = "expr";
            }
            if (is(NodeType::stmt)) {
                vec[i++] = "stmt";
            }
            if (is(NodeType::type)) {
                vec[i++] = "type";
            }
            vec[i++] = "node";

            struct {
                const char* const* start;
                const char* const* finish;

                const char* const* begin() const {
                    return start;
                }

                const char* const* end() const {
                    return finish;
                }
            } p{vec, vec + i};
            field("base_node_type", p);
        }

        template <class T>
        void list_derive_type(auto&& field) {
            const char* vec[node_type_count]{};
            size_t i = 0;
            for (size_t j = 0; j < node_type_count; j++) {
                get_node(*mapValueToNodeType_2(j), [&](auto node) {
                    using P = typename decltype(node)::node;
                    if constexpr (std::is_base_of_v<T, P> && !std::is_same_v<T, P>) {
                        vec[i++] = node_type_to_string(P::node_type_tag);
                    }
                });
            }
            if (i != 0) {
                struct {
                    const char* const* start;
                    const char* const* finish;

                    const char* const* begin() const {
                        return start;
                    }

                    const char* const* end() const {
                        return finish;
                    }
                } p{vec, vec + i};
                field("one_of", p);
            }
        }

        template <bool ast_mode, class T>
        void do_dump(T& t, auto&& field) {
            auto do_dump = [&](auto&& field) {
                t.dump([&](std::string_view key, auto& v) {
                    using P = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<P, NodeType>) {
                        // nothing to do
                    }
                    else if constexpr (std::is_same_v<P, bool>) {
                        field(key, "bool");
                    }
                    else if constexpr (std::is_same_v<P, size_t>) {
                        field(key, "uint");
                    }
                    else if constexpr (std::is_same_v<P, std::optional<size_t>>) {
                        field(key, "optional<uint>");
                    }
                    else if constexpr (std::is_same_v<P, std::string>) {
                        field(key, "string");
                    }
                    else if constexpr (std::is_same_v<P, lexer::Loc>) {
                        if (key != "loc") {
                            field(key, "loc");
                        }
                    }
                    else if constexpr (std::is_same_v<P, UnaryOp>) {
                        field(key, "unary_op");
                    }
                    else if constexpr (std::is_same_v<P, BinaryOp>) {
                        field(key, "binary_op");
                    }
                    else if constexpr (std::is_same_v<P, IdentUsage>) {
                        field(key, "ident_usage");
                    }
                    else if constexpr (std::is_same_v<P, Endian>) {
                        field(key, "endian");
                    }
                    else if constexpr (std::is_same_v<P, ConstantLevel>) {
                        field(key, "constant_level");
                    }
                    else if constexpr (std::is_same_v<P, BitAlignment>) {
                        field(key, "bit_alignment");
                    }
                    else if constexpr (std::is_same_v<P, Follow>) {
                        field(key, "follow");
                    }
                    else if constexpr (std::is_same_v<P, IOMethod>) {
                        field(key, "io_method");
                    }
                    else if constexpr (std::is_same_v<P, SpecialLiteralKind>) {
                        field(key, "special_literal_kind");
                    }
                    else if constexpr (futils::helper::is_template<P>) {
                        using P1 = typename futils::helper::template_of_t<P>::template param_at<0>;
                        if constexpr (futils::helper::is_template_instance_of<P, std::shared_ptr>) {
                            auto p = internal::print_ptr<ast_mode, P1>("shared_ptr");
                            field(key, internal::unwrap(p));
                        }
                        else if constexpr (futils::helper::is_template_instance_of<P, std::weak_ptr>) {
                            auto p = internal::print_ptr<ast_mode, P1>("weak_ptr");
                            field(key, internal::unwrap(p));
                        }
                        else if constexpr (futils::helper::is_template_instance_of<P, std::vector>) {
                            auto p = internal::print_array<ast_mode, P1>();
                            field(key, internal::unwrap(p));
                        }
                        else {
                            static_assert(std::is_same_v<P, NodeType>);
                        }
                    }
                    else {
                        static_assert(std::is_same_v<P, NodeType>);
                    }
                });
            };
            t.dump([&](std::string_view key, auto& v) {
                using P = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<P, NodeType>) {
                    field(key, node_type_to_string(T::node_type_tag));
                    internal::dump_base_type(field, T::node_type_tag);
                }
                else if constexpr (std::is_same_v<P, lexer::Loc>) {
                    if (key == "loc") {
                        field(key, "loc");
                    }
                }
            });
            field("body", [&](auto&& f) {
                auto field = f.object();
                do_dump(field);
            });
        }

        template <class T>
        struct Dummy : T {
            Dummy()
                : T({}, T::node_type_tag) {}
        };
    }  // namespace internal

    constexpr auto scope_type_list = R"({"prev": "weak_ptr<scope>","next": "shared_ptr<scope>","branch": "shared_ptr<scope>","ident": "array<std::weak_ptr<ident>>","owner": "weak_ptr<node>", "branch_root": "bool"})";
    constexpr auto raw_scope_type = R"({"prev": "optional<uintptr>","next": "optional<uintptr>","branch": "optional<uintptr>","ident": "array<uintptr>", "owner": "optional<uintptr>","branch_root": "bool"})";

    constexpr auto loc_type = R"({"pos": "pos","file": "uint","line": "uint","col": "uint"})";
    constexpr auto pos_type = R"({"begin": "uint","end": "uint"})";

    constexpr auto token_type = R"({"tag": "token_tag","token": "string","loc": "loc"})";
    constexpr auto src_error_entry_type = R"({"msg": "string","file": "string","loc": "loc","src": "string","warn": "bool"})";
    constexpr auto src_error_type = R"({"errs": "array<src_error_entry>"})";

    constexpr auto raw_node_type = R"({"node_type": "node_type","loc": "loc","body": "any"})";

    constexpr auto json_ast = R"({"node": "array<raw_node>","scope": "array<raw_scope>"})";
    constexpr auto ast_file = R"({"files": "array<string>","ast": "optional<json_ast>","error": "optional<src_error>"})";
    constexpr auto token_file = R"({"files": "array<string>","tokens": "optional<array<token>>","error": "optional<src_error>"})";

    template <bool ast_mode = false>
    void node_types(auto&& objdump) {
        objdump([&](auto&& field) {
            field("node_type", "node");
            internal::list_derive_type<Node>(field);
        });
        for (size_t i = 0; i < node_type_count; i++) {
            get_node(*mapValueToNodeType_2(i), [&](auto node) {
                using T = typename decltype(node)::node;
                if constexpr (std::is_default_constructible_v<T>) {
                    objdump([&](auto&& field) {
                        T t;
                        internal::do_dump<ast_mode>(t, field);
                    });
                }
                else {
                    objdump([&](auto&& field) {
                        internal::Dummy<T> t;
                        internal::do_dump<ast_mode>(t, field);
                        internal::list_derive_type<T>(field);
                    });
                }
            });
        }
    }

    template <class T>
    void enum_type(const char* key, auto&& field) {
        field(key, [&](auto&& d) {
            auto field = d.array();
            for (size_t i = 0; i < enum_elem_count<T>(); i++) {
                field([&](auto&& d) {
                    auto field = d.object();
                    field("name", enum_name_array<T>[i].second);
                    field("value", enum_array<T>[i].second);
                });
            }
        });
    }

    void enum_types(auto&& field) {
        {
            field("node_type", [&](auto&& d) {
                auto field = d.array();
                for (size_t i = 0; i < node_type_count; i++) {
                    field([&](auto&& d) {
                        auto field = d.object();
                        field("name", sorted_node_type_str_array[i].second);
                        field("value", sorted_node_type_str_array[i].second);
                    });
                }
            });
        }
        field("token_tag", [&](auto&& d) {
            auto field = d.array();
            for (size_t i = 0; i < lexer::enum_elem_count<lexer::Tag>(); i++) {
                field([&](auto&& d) {
                    auto field = d.object();
                    field("name", lexer::enum_name_array<lexer::Tag>[i].second);
                    field("value", lexer::enum_array<lexer::Tag>[i].second);
                });
            }
        });
        enum_type<UnaryOp>("unary_op", field);
        enum_type<BinaryOp>("binary_op", field);
        enum_type<IdentUsage>("ident_usage", field);
        enum_type<Endian>("endian", field);
        enum_type<ConstantLevel>("constant_level", field);
        enum_type<BitAlignment>("bit_alignment", field);
        enum_type<Follow>("follow", field);
        enum_type<IOMethod>("io_method", field);
        enum_type<SpecialLiteralKind>("special_literal_kind", field);
    }

    void struct_types(auto&& field) {
        field("scope", futils::json::RawJSON<const char*>{scope_type_list});
        field("pos", futils::json::RawJSON<const char*>{brgen::ast::pos_type});
        field("loc", futils::json::RawJSON<const char*>{brgen::ast::loc_type});
        field("token", futils::json::RawJSON<const char*>{brgen::ast::token_type});
        field("raw_scope", futils::json::RawJSON<const char*>{raw_scope_type});
        field("raw_node", futils::json::RawJSON<const char*>{raw_node_type});
        field("src_error_entry", futils::json::RawJSON<const char*>{brgen::ast::src_error_entry_type});
        field("src_error", futils::json::RawJSON<const char*>{brgen::ast::src_error_type});
        field("json_ast", futils::json::RawJSON<const char*>{brgen::ast::json_ast});
        field("ast_file", futils::json::RawJSON<const char*>{brgen::ast::ast_file});
        field("token_file", futils::json::RawJSON<const char*>{brgen::ast::token_file});
    }
}  // namespace brgen::ast
