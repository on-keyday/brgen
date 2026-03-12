/*license*/
#pragma once

#include <string_view>
#include <type_traits>
#include "access_helper.hpp"  // auto generated
#include "ebm/extended_binary_module.hpp"
#include "number/parse.h"
namespace ebmgen {

    struct DotCount {
        size_t count = 0;
        consteval DotCount(const char* n) {
            for (auto c : std::string_view(n)) {
                if (c == '.') {
                    count++;
                }
            }
        }
    };

    struct FieldOnlyTag {
        bool is_array_index = false;
        size_t index = 0;
    };

    struct NameTag {
        FieldOnlyTag field_tag;
        ebmcodegen::TypeIndex type_index{};
    };

    static constexpr std::string_view next_field(std::string_view& remain) {
        size_t found = remain.find('.');
        auto name = remain.substr(0, found);
        remain = remain.substr(name.size() + (found != std::string_view::npos ? 1 : 0));
        return name;
    }

    constexpr auto instance_index = static_cast<size_t>(-2);
    constexpr auto optional_index = static_cast<size_t>(-3);

    template <size_t N = 10>
    struct FieldNames {
        constexpr static size_t max_depth = N;
        FieldOnlyTag tags[N]{};
        size_t depth = 0;
        constexpr void add_tag(const FieldOnlyTag& tag) {
            if (depth >= N) {
                throw "Too deep name nesting";
            }
            tags[depth++] = tag;
        }

        consteval FieldNames(const char* n) {
            size_t dot_count = DotCount(n).count;
            if (dot_count >= N) {
                throw "Too deep name nesting";
            }
            std::string_view remain = n;
            while (!remain.empty()) {
                auto field_name = next_field(remain);
                auto name = ebmcodegen::get_field_index(field_name);
                FieldOnlyTag tag{};
                if (name == static_cast<size_t>(-1)) {
                    if (field_name == "instance") {
                        tag.index = instance_index;
                    }
                    else if (field_name == "optional") {
                        tag.index = optional_index;
                    }
                    else {
                        if (!futils::number::parse_integer(field_name, tag.index)) {
                            throw "Invalid field name or array index";
                        }
                        tag.is_array_index = true;
                    }
                }
                else {
                    tag.index = name;
                }
                add_tag(tag);
            }
        }
    };

    template <size_t N = 10>
    struct Name {
        NameTag tags[N]{};
        size_t depth = 0;
        constexpr void add_tag(const NameTag& tag) {
            if (depth >= N) {
                throw "Too deep name nesting";
            }
            tags[depth++] = tag;
        }

        static constexpr auto type_index_statement = ebmcodegen::get_type_index<ebm::Statement>();
        static constexpr auto type_index_statement_body = ebmcodegen::get_type_index<ebm::StatementBody>();
        static constexpr auto type_index_type = ebmcodegen::get_type_index<ebm::Type>();
        static constexpr auto type_index_type_body = ebmcodegen::get_type_index<ebm::TypeBody>();
        static constexpr auto type_index_expression = ebmcodegen::get_type_index<ebm::Expression>();
        static constexpr auto type_index_expression_body = ebmcodegen::get_type_index<ebm::ExpressionBody>();
        static constexpr auto type_index_statement_ref = ebmcodegen::get_type_index<ebm::StatementRef>();
        static constexpr auto type_index_type_ref = ebmcodegen::get_type_index<ebm::TypeRef>();
        static constexpr auto type_index_expression_ref = ebmcodegen::get_type_index<ebm::ExpressionRef>();

        // weak refs
        static constexpr auto type_index_weak_statement_ref = ebmcodegen::get_type_index<ebm::WeakStatementRef>();
        static constexpr auto type_index_lowered_statement_ref = ebmcodegen::get_type_index<ebm::LoweredStatementRef>();
        static constexpr auto type_index_lowered_expression_ref = ebmcodegen::get_type_index<ebm::LoweredExpressionRef>();

        static constexpr auto field_index_body = ebmcodegen::get_field_index("body");

        consteval Name(size_t root_index, FieldNames<N> n) {
            ebmcodegen::TypeIndex type_index{.index = root_index};
            size_t i = 0;
            while (i < n.depth) {
                if (n.tags[i].is_array_index || type_index.is_array) {
                    if (!n.tags[i].is_array_index || !type_index.is_array) {
                        [](auto... a) { throw "Invalid field name or array index"; }(
                            ebmcodegen::type_name_from_index(type_index.index),
                            n.tags[i],
                            type_index, i);
                    }
                    type_index.is_array = false;
                    add_tag({NameTag{.field_tag = n.tags[i], .type_index = type_index}});
                    i++;
                    if (i == n.depth) {
                        break;
                    }
                }
                auto new_type_index = ebmcodegen::get_type_index_from_field(type_index.index, n.tags[i].index);
                if (new_type_index.index == static_cast<size_t>(-1)) {
                    auto do_fallback = [&](size_t stmt, size_t body) {  // fallback to body field
                        new_type_index = ebmcodegen::get_type_index_from_field(stmt, n.tags[i].index);
                        if (new_type_index.index == static_cast<size_t>(-1)) {
                            new_type_index = ebmcodegen::get_type_index_from_field(body, n.tags[i].index);
                            if (new_type_index.index != static_cast<size_t>(-1)) {
                                ebmcodegen::TypeIndex stmt_index{.index = type_index_statement};
                                auto copy = n.tags[i];
                                copy.index = field_index_body;
                                add_tag({NameTag{.field_tag = copy, .type_index = stmt_index}});  // adjust to body field
                            }
                            else if (n.tags[i].index == instance_index) {
                                new_type_index = type_index;  // stay at the same type
                            }
                        }
                    };
                    if (type_index.index == type_index_statement_ref || type_index.index == type_index_weak_statement_ref || type_index.index == type_index_lowered_statement_ref) {
                        do_fallback(type_index_statement, type_index_statement_body);
                    }
                    else if (type_index.index == type_index_type_ref) {
                        do_fallback(type_index_type, type_index_type_body);
                    }
                    else if (type_index.index == type_index_expression_ref || type_index.index == type_index_lowered_expression_ref) {
                        do_fallback(type_index_expression, type_index_expression_body);
                    }
                    if (new_type_index.index == static_cast<size_t>(-1) && n.tags[i].index == optional_index) {
                        new_type_index = type_index;  // stay at same type
                    }
                    if (new_type_index.index == static_cast<size_t>(-1)) {
                        [](auto... a) { throw "Invalid field name"; }(
                            ebmcodegen::type_name_from_index(type_index.index),
                            ebmcodegen::field_name_from_index(n.tags[i].index));
                    }
                }
                add_tag({NameTag{.field_tag = n.tags[i], .type_index = type_index}});
                type_index = new_type_index;
                i++;
            }
        }

        template <class T>
        static consteval Name make(FieldNames<N> n) {
            return Name{ebmcodegen::get_type_index<std::decay_t<T>>(), n};
        }
    };

    template <size_t N, Name<N> V, size_t offset = 0, size_t step = 0>
    struct FieldAccessor {
        static constexpr auto field_index = V.tags[offset].field_tag.index;

        static constexpr auto get_type_index() {
            auto last = V.tags[V.depth - 1];
            return last.type_index;
        }

        using R = std::decay_t<decltype(ebmcodegen::type_from_index<get_type_index()>())>;

        static constexpr decltype(auto) get_field(auto&& ctx, auto&& in) {
            using T = std::decay_t<decltype(in)>;
#define REF_TO_INSTANCE(RefType, Instance, getter_name)                                                                                     \
    else if constexpr (std::is_same_v<T, RefType>) {                                                                                        \
        return FieldAccessor<N, V, offset, step + 1>::get_field(ctx, ctx.get_##getter_name(in));                                            \
    }                                                                                                                                       \
    else if constexpr (std::is_same_v<T, RefType*> || std::is_same_v<T, const RefType*>) {                                                  \
        if (!in) {                                                                                                                          \
            using ResultT = std::decay_t<decltype(FieldAccessor<N, V, offset, step + 1>::get_field(ctx, *static_cast<RefType*>(nullptr)))>; \
            return ResultT{};                                                                                                               \
        }                                                                                                                                   \
        return FieldAccessor<N, V, offset, step + 1>::get_field(ctx, *in);                                                                  \
    }

#define WEAK_REF_TO_INSTANCE(RefType)                                                                                 \
    else if constexpr (std::is_same_v<T, RefType>) {                                                                  \
        return FieldAccessor<N, V, offset, step + 1>::get_field(ctx, in.id);                                          \
    }                                                                                                                 \
    else if constexpr (std::is_same_v<T, RefType*> || std::is_same_v<T, const RefType*>) {                            \
        if (!in) {                                                                                                    \
            using ResultT = std::decay_t<decltype(FieldAccessor<N, V, offset, step + 1>::get_field(ctx, RefType{}))>; \
            return ResultT{};                                                                                         \
        }                                                                                                             \
        return FieldAccessor<N, V, offset, step + 1>::get_field(ctx, in->id);                                         \
    }

            if constexpr (false) {
            }
            REF_TO_INSTANCE(ebm::StatementRef, ebm::Statement, statement)
            REF_TO_INSTANCE(ebm::TypeRef, ebm::Type, type)
            REF_TO_INSTANCE(ebm::ExpressionRef, ebm::Expression, expression)
            WEAK_REF_TO_INSTANCE(ebm::WeakStatementRef)
            WEAK_REF_TO_INSTANCE(ebm::LoweredStatementRef)
            WEAK_REF_TO_INSTANCE(ebm::LoweredExpressionRef)
#undef REF_TO_INSTANCE
            else if constexpr (offset + 1 < V.depth) {
                if constexpr (V.tags[offset].field_tag.is_array_index) {
                    using ResultT = std::decay_t<decltype(FieldAccessor<N, V, offset + 1, step + 1>::get_field(ctx, std::addressof((*in)[V.tags[offset].field_tag.index])))>;
                    if constexpr (std::is_pointer_v<std::decay_t<decltype(in)>>) {
                        if (!in) {
                            return ResultT{};
                        }
                        if (in->size() <= V.tags[offset].field_tag.index) {
                            return ResultT{};
                        }
                        return FieldAccessor<N, V, offset + 1, step + 1>::get_field(ctx, std::addressof((*in)[V.tags[offset].field_tag.index]));
                    }
                    else {
                        if (in.size() <= V.tags[offset].field_tag.index) {
                            return ResultT{};
                        }
                        return FieldAccessor<N, V, offset + 1, step + 1>::get_field(ctx, std::addressof(in[V.tags[offset].field_tag.index]));
                    }
                }
                else {
                    if constexpr (V.tags[offset].field_tag.index == instance_index) {
                        return FieldAccessor<N, V, offset + 1, step + 1>::get_field(ctx, in);
                    }
                    else {
                        return FieldAccessor<N, V, offset + 1, step + 1>::get_field(ctx, ebmcodegen::get_field<field_index>(in));
                    }
                }
            }
            else {
                if constexpr (V.tags[offset].field_tag.is_array_index) {
                    using ResultT = std::decay_t<decltype(std::addressof((*in)[V.tags[offset].field_tag.index]))>;
                    if constexpr (std::is_pointer_v<std::decay_t<decltype(in)>>) {
                        if (!in) {
                            return ResultT{};
                        }
                        if (in->size() <= V.tags[offset].field_tag.index) {
                            return ResultT{};
                        }
                        return std::addressof((*in)[V.tags[offset].field_tag.index]);
                    }
                    else {
                        if (in.size() <= V.tags[offset].field_tag.index) {
                            return ResultT{};
                        }
                        return std::addressof(in[V.tags[offset].field_tag.index]);
                    }
                }
                else {
                    if constexpr (V.tags[offset].field_tag.index == instance_index) {
                        if constexpr (std::is_pointer_v<std::decay_t<decltype(in)>>) {
                            return (std::decay_t<decltype(in)>)(in);
                        }
                        else {
                            auto& ref = in;
                            return ref;
                        }
                    }
                    else if constexpr (V.tags[offset].field_tag.index == optional_index) {
                        if constexpr (std::is_pointer_v<std::decay_t<decltype(in)>>) {
                            if (!in) {
                                return std::optional<std::decay_t<decltype(*in)>>{std::nullopt};
                            }
                            return std::optional<std::decay_t<decltype(*in)>>(*in);
                        }
                        else {
                            auto& ref = in;
                            return ref;
                        }
                    }
                    else {
                        return ebmcodegen::get_field<field_index>(in);
                    }
                }
            }
        }
    };

    template <size_t N, FieldNames<N> V>
    constexpr decltype(auto) access_field(auto&& ctx, auto&& in) {
        constexpr auto name = Name<N>::template make<std::remove_pointer_t<std::decay_t<decltype(in)>>>(V);
        return FieldAccessor<N, name>::get_field(ctx, in);
    }

    template <FieldNames<10> V>
    constexpr decltype(auto) access_field(auto&& ctx, auto&& in) {
        return access_field<V.max_depth, V>(ctx, in);
    }

    namespace test {
        struct DummyContext {
            constexpr ebm::Statement* get_statement(ebm::StatementRef& ref) const {
                return nullptr;
            }
            constexpr ebm::Type* get_type(ebm::TypeRef& ref) const {
                return nullptr;
            }
            constexpr ebm::Expression* get_expression(ebm::ExpressionRef& ref) const {
                return nullptr;
            }
        };
        constexpr auto access_name = "statements.0.body.field_decl.field_type.body.kind";

        constexpr auto v = access_field<access_name>(DummyContext{}, static_cast<ebm::ExtendedBinaryModule*>(nullptr));

        inline void usage() {
            ebm::ExtendedBinaryModule mod;
            auto& statements = access_field<"statements">(DummyContext{}, mod);
            auto& statement = statements[0];
            auto f = access_field<"body.field_decl">(DummyContext{}, statement);
            if (f) {
                auto kind = access_field<"field_type.kind">(DummyContext{}, *f);
            }
            auto x = access_field<"statements.0">(DummyContext{}, &mod);

            auto is_optional = access_field<"is_state_variable">(DummyContext{}, &*f);

            auto instance = access_field<"id.instance">(DummyContext{}, statement);

            auto ptr_to_optional = access_field<"id.kind.optional">(DummyContext{}, statement);
        }

    }  // namespace test

}  // namespace ebmgen
