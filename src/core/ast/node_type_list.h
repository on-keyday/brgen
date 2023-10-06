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
                utils::number::Array<char, 50, true> buf{};
                utils::strutil::appends(buf, prefix, "uintptr", suffix);
                return buf;
            }
            else if constexpr (std::is_base_of_v<Node, P1>) {
                utils::number::Array<char, 50, true> buf{};
                utils::strutil::appends(buf, prefix, ptr_annot, "<");
                if constexpr (std::is_same_v<Node, P1>) {
                    utils::strutil::append(buf, "node");
                }
                else {
                    utils::strutil::append(buf, node_type_to_string(P1::node_type_tag));
                }
                utils::strutil::appends(buf, ">", suffix);
                return buf;
            }
            else if constexpr (std::is_same_v<Scope, P1>) {
                utils::number::Array<char, 50, true> buf{};
                utils::strutil::appends(buf, prefix, ptr_annot, "<scope>", suffix);
                return buf;
            }
        }

        template <bool ast_mode, class P1>
        auto print_array() {
            if constexpr (utils::helper::is_template<P1>) {
                using P2 = typename utils::helper::template_of_t<P1>::template param_at<0>;
                if constexpr (utils::helper::is_template_instance_of<P1, std::shared_ptr>) {
                    auto p = print_ptr<ast_mode, P2>("shared_ptr", "array<", ">");
                    return p;
                }
                else if constexpr (utils::helper::is_template_instance_of<P1, std::weak_ptr>) {
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
            const char* vec[6]{};
            size_t i = 0;
            auto t = int(type);
            auto is = [&](auto v) { return t != int(v) && (t & int(v)) == int(v); };

            if (is(NodeType::literal)) {
                vec[i++] = "literal";
            }
            if (is(NodeType::member)) {
                vec[i++] = "member";
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
                get_node(*mapValueToNodeType(j), [&](auto node) {
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
        void do_dump(T& t, auto&& field, bool flat, bool dump_base) {
            auto do_dump = [&](auto&& field) {
                t.dump([&](std::string_view key, auto& v) {
                    using P = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<P, NodeType>) {
                        if (flat) {
                            field(key, node_type_to_string(T::node_type_tag));
                            if (dump_base) {
                                internal::dump_base_type(field, T::node_type_tag);
                            }
                        }
                    }
                    else if constexpr (std::is_same_v<P, bool>) {
                        field(key, "bool");
                    }
                    else if constexpr (std::is_same_v<P, size_t>) {
                        field(key, "uint");
                    }
                    else if constexpr (std::is_same_v<P, std::string>) {
                        field(key, "string");
                    }
                    else if constexpr (std::is_same_v<P, lexer::Loc>) {
                        if (flat || key != "loc") {
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
                    else if constexpr (utils::helper::is_template<P>) {
                        using P1 = typename utils::helper::template_of_t<P>::template param_at<0>;
                        if constexpr (utils::helper::is_template_instance_of<P, std::shared_ptr>) {
                            auto p = internal::print_ptr<ast_mode, P1>("shared_ptr");
                            field(key, internal::unwrap(p));
                        }
                        else if constexpr (utils::helper::is_template_instance_of<P, std::weak_ptr>) {
                            auto p = internal::print_ptr<ast_mode, P1>("weak_ptr");
                            field(key, internal::unwrap(p));
                        }
                        else if constexpr (utils::helper::is_template_instance_of<P, std::list> ||
                                           utils::helper::is_template_instance_of<P, std::vector>) {
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
            if (!flat) {
                t.dump([&](std::string_view key, auto& v) {
                    using P = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<P, NodeType>) {
                        field(key, node_type_to_string(T::node_type_tag));
                        if (dump_base) {
                            internal::dump_base_type(field, T::node_type_tag);
                        }
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
            else {
                do_dump(field);
            }
        }

        template <class T>
        struct Dummy : T {
            Dummy()
                : T({}, T::node_type_tag) {}
        };
    }  // namespace internal

    constexpr auto scope_type_list = R"({"prev": "weak_ptr<scope>","next": "shared_ptr<scope>","branch": "shared_ptr<scope>","ident": "array<std::weak_ptr<node>>"})";
    constexpr auto scope_type_ast_mode_list = R"({"prev": "uintptr","next": "uintptr","branch": "uintptr","ident": "array<uintptr>"})";

    constexpr auto loc_type = R"({"pos": "pos","file": "uint","line": "uint","col": "uint"})";
    constexpr auto pos_type = R"({"begin": "uint","end": "uint"})";

    template <bool ast_mode = false>
    void node_type_list(auto&& objdump, bool flat = false, bool dump_base = true) {
        objdump([&](auto&& field) {
            field("node_type", "node");
            internal::list_derive_type<Node>(field);
        });
        for (size_t i = 0; i < node_type_count; i++) {
            get_node(*mapValueToNodeType(i), [&](auto node) {
                using T = typename decltype(node)::node;
                if constexpr (std::is_default_constructible_v<T>) {
                    /*
                    objdump([&](auto&& field) {
                        T t;
                        auto do_dump = [&](auto&& field) {
                            t.dump([&](std::string_view key, auto& v) {
                                using P = std::decay_t<decltype(v)>;
                                if constexpr (std::is_same_v<P, NodeType>) {
                                    if (flat) {
                                        field(key, node_type_to_string(T::node_type_tag));
                                        if (dump_base) {
                                            internal::dump_base_type(field, T::node_type_tag);
                                        }
                                    }
                                }
                                else if constexpr (std::is_same_v<P, bool>) {
                                    field(key, "bool");
                                }
                                else if constexpr (std::is_same_v<P, size_t>) {
                                    field(key, "uint");
                                }
                                else if constexpr (std::is_same_v<P, std::string>) {
                                    field(key, "string");
                                }
                                else if constexpr (std::is_same_v<P, lexer::Loc>) {
                                    if (flat || key != "loc") {
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
                                else if constexpr (utils::helper::is_template<P>) {
                                    using P1 = typename utils::helper::template_of_t<P>::template param_at<0>;
                                    if constexpr (utils::helper::is_template_instance_of<P, std::shared_ptr>) {
                                        auto p = internal::print_ptr<ast_mode, P1>("shared_ptr");
                                        field(key, internal::unwrap(p));
                                    }
                                    else if constexpr (utils::helper::is_template_instance_of<P, std::weak_ptr>) {
                                        auto p = internal::print_ptr<ast_mode, P1>("weak_ptr");
                                        field(key, internal::unwrap(p));
                                    }
                                    else if constexpr (utils::helper::is_template_instance_of<P, std::list> ||
                                                       utils::helper::is_template_instance_of<P, std::vector>) {
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
                        if (!flat) {
                            t.dump([&](std::string_view key, auto& v) {
                                using P = std::decay_t<decltype(v)>;
                                if constexpr (std::is_same_v<P, NodeType>) {
                                    field(key, node_type_to_string(T::node_type_tag));
                                    if (dump_base) {
                                        internal::dump_base_type(field, T::node_type_tag);
                                    }
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
                        else {
                            do_dump(field);
                        }
                    });*/
                    objdump([&](auto&& field) {
                        T t;
                        internal::do_dump<ast_mode>(t, field, flat, dump_base);
                    });
                }
                else {
                    objdump([&](auto&& field) {
                        internal::Dummy<T> t;
                        internal::do_dump<ast_mode>(t, field, flat, dump_base);
                        internal::list_derive_type<T>(field);
                    });
                }
            });
        }
    }

    void custom_type_mapping(auto&& field, bool ast_mode = false, bool enum_name = false) {
        struct R {
            const char* const* start;
            const char* const* finish;

            const char* const* begin() const {
                return start;
            }

            const char* const* end() const {
                return finish;
            }
        };
        {
            R p{unary_op_str, unary_op_str + unary_op_count};
            if (enum_name) {
                field("unary_op", [&](auto&& d) {
                    auto field = d.array();
                    for (size_t i = 0; i < unary_op_count; i++) {
                        field([&](auto&& d) {
                            auto field = d.object();
                            field("op", unary_op_str[i]);
                            field("name", unary_op_name[i]);
                        });
                    }
                });
            }
            else {
                field("unary_op", p);
            }
        }
        {
            R p{bin_op_list, bin_op_list + bin_op_count};
            if (enum_name) {
                field("binary_op", [&](auto&& d) {
                    auto field = d.array();
                    for (size_t i = 0; i < bin_op_count; i++) {
                        field([&](auto&& d) {
                            auto field = d.object();
                            field("op", bin_op_list[i]);
                            field("name", bin_op_name[i]);
                        });
                    }
                });
            }
            else {
                field("binary_op", p);
            }
        }
        {
            R p{ident_usage_str, ident_usage_str + ident_usage_count};
            field("ident_usage", p);
        }
        {
            R p{endian_str, endian_str + endian_count};
            field("endian", p);
        }
        if (ast_mode) {
            field("scope", utils::json::RawJSON<const char*>{scope_type_ast_mode_list});
        }
        else {
            field("scope", utils::json::RawJSON<const char*>{scope_type_list});
        }
        field("loc", utils::json::RawJSON<const char*>{brgen::ast::loc_type});
        field("pos", utils::json::RawJSON<const char*>{brgen::ast::pos_type});
    }
}  // namespace brgen::ast
