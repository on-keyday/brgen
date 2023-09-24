/*license*/
#pragma once

#include "traverse.h"
#include <number/array.h>
#include <strutil/append.h>

namespace brgen::ast {
    namespace internal {
        template <class P1>
        auto print_ptr(const char* prefix, const char* suffix = "") {
            if constexpr (std::is_base_of_v<Node, P1>) {
                utils::number::Array<char, 50, true> buf{};
                utils::strutil::appends(buf, prefix, "<");
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
                return "shared_ptr<scope>";
            }
        }

        template <class P1>
        auto print_array() {
            if constexpr (utils::helper::is_template<P1>) {
                using P2 = typename utils::helper::template_of_t<P1>::template param_at<0>;
                if constexpr (utils::helper::is_template_instance_of<P1, std::shared_ptr>) {
                    auto p = print_ptr<P2>("array<shared_ptr", ">");
                    return p;
                }
                else if constexpr (utils::helper::is_template_instance_of<P1, std::weak_ptr>) {
                    auto p = print_ptr<P2>("array<weak_ptr", ">");
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
            const char* vec[5]{};
            size_t i = 0;
            auto t = int(type);
            auto is = [&](auto v) { return (t & int(v)) == int(v); };
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
                field("base_node_type", p);
            }
        }
    }  // namespace internal

    void type_list(auto&& objdump) {
        for (size_t i = 0; i < node_type_count; i++) {
            get_node(*mapValueToNodeType(i), [&](auto node) {
                using T = typename decltype(node)::node;
                if constexpr (std::is_default_constructible_v<T>) {
                    objdump([&](auto&& field) {
                        T t;
                        t.dump([&](const char* key, auto& v) {
                            using P = std::decay_t<decltype(v)>;
                            if constexpr (std::is_same_v<P, NodeType>) {
                                field(key, node_type_to_string(T::node_type_tag));
                                internal::dump_base_type(field, T::node_type_tag);
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
                                field(key, "loc");
                            }
                            else if constexpr (std::is_same_v<P, UnaryOp>) {
                                struct {
                                    const char* const* start;
                                    const char* const* finish;

                                    const char* const* begin() const {
                                        return start;
                                    }

                                    const char* const* end() const {
                                        return finish;
                                    }
                                } p{unary_op_str, unary_op_str + unary_op_count};
                                field(key, p);
                            }
                            else if constexpr (std::is_same_v<P, BinaryOp>) {
                                struct {
                                    const char* const* start;
                                    const char* const* finish;

                                    const char* const* begin() const {
                                        return start;
                                    }

                                    const char* const* end() const {
                                        return finish;
                                    }
                                } p{bin_op_list, bin_op_list + bin_op_count};
                                field(key, p);
                            }
                            else if constexpr (std::is_same_v<P, IdentUsage>) {
                                struct {
                                    const char* const* start;
                                    const char* const* finish;
                                    const char* const* begin() const {
                                        return start;
                                    }

                                    const char* const* end() const {
                                        return finish;
                                    }
                                } p{ident_usage_str, ident_usage_str + ident_usage_count};
                                field(key, p);
                            }
                            else if constexpr (utils::helper::is_template<P>) {
                                using P1 = typename utils::helper::template_of_t<P>::template param_at<0>;
                                if constexpr (utils::helper::is_template_instance_of<P, std::shared_ptr>) {
                                    auto p = internal::print_ptr<P1>("shared_ptr");
                                    field(key, internal::unwrap(p));
                                }
                                else if constexpr (utils::helper::is_template_instance_of<P, std::weak_ptr>) {
                                    auto p = internal::print_ptr<P1>("weak_ptr");
                                    field(key, internal::unwrap(p));
                                }
                                else if constexpr (utils::helper::is_template_instance_of<P, std::list> ||
                                                   utils::helper::is_template_instance_of<P, std::vector>) {
                                    auto p = internal::print_array<P1>();
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
                    });
                }
            });
        }
    }
}  // namespace brgen::ast
