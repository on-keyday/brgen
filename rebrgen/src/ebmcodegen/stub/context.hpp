/*license*/
#pragma once
#include "comb2/composite/range.h"
#include "core/sequencer.h"
#include <ebmgen/common.hpp>
#include "ebmgen/access.hpp"

namespace ebmcodegen::util {

    namespace internal {
        constexpr bool is_c_ident(const char* ident) {
            auto seq = futils::make_ref_seq(ident);
            auto res = futils::comb2::composite::c_ident(seq, 0, 0);
            return res == futils::comb2::Status::match && seq.eos();
        }

        constexpr auto force_wrap_expected(auto&& value) {
            if constexpr (std::is_pointer_v<std::decay_t<decltype(value)>> || futils::helper::is_template_instance_of<std::decay_t<decltype(value)>, futils::helper::either::expected>) {
                return value;
            }
            else {
                return ebmgen::expected<std::decay_t<decltype(value)>>{std::forward<decltype(value)>(value)};
            }
        }
    }  // namespace internal

    // This is for signaling continue normal processing without error
    constexpr auto pass = futils::helper::either::unexpected{ebmgen::Error(futils::error::Category::lib, 0xba55ba55)};
    constexpr bool is_pass_error(const ebmgen::Error& err) {
        return err.category() == futils::error::Category::lib && err.sub_category() == 0xba55ba55;
    }

    constexpr bool needs_return(auto&& result) {
        return result.has_value() || !is_pass_error(result.error());
    }

    constexpr auto wrap_return(auto&& result, std::source_location cur = std::source_location::current()) -> std::decay_t<decltype(result)> {
        if (result.has_value()) {
            return result;
        }
        return ebmgen::unexpect_error_with_loc(cur, std::move(result.error()));
    }
    // CALL_OR_PASS macro: call a function that returns expected<T>, if it returns pass error, continue, otherwise return the error or value
#define CALL_OR_PASS(uniq, func_call)              \
    auto uniq##_res = (func_call);                 \
    if (needs_return(uniq##_res)) {                \
        return wrap_return(std::move(uniq##_res)); \
    }

    template <class ResultType>
    struct MainLogicWrapper {
       private:
        ebmgen::expected<ResultType> (*main_logic_)(void*);
        void* arg_;

        template <class F>
        constexpr static ebmgen::expected<ResultType> invoke_main_logic(void* arg) {
            auto* self = static_cast<F*>(arg);
            return (*self)();
        }

       public:
        template <class F>
        constexpr MainLogicWrapper(F&& f)
            : main_logic_(&invoke_main_logic<std::decay_t<F>>), arg_(static_cast<void*>(std::addressof(f))) {}

        constexpr ebmgen::expected<ResultType> operator()() {
            return main_logic_(arg_);
        }
    };

    template <class D>
    concept has_item_id = requires(D d) {
        { d.item_id };
    };

    template <class D, class T>
    concept has_item_id_typed = requires(D d) {
        { d.item_id } -> std::convertible_to<T>;
    };

    template <class D>
    concept has_type = requires(D d) {
        { d.type } -> std::convertible_to<ebm::TypeRef>;
    };

    template <class D>
    struct ContextBase {
       private:
        D& derived() {
            return static_cast<D&>(*this);
        }

        const D& derived() const {
            return static_cast<const D&>(*this);
        }

       public:
        decltype(auto) identifier() const
            requires has_item_id<D>
        {
            return derived().visitor.module_.get_associated_identifier(derived().item_id);
        }

        decltype(auto) identifier(ebm::StatementRef stmt) const {
            return derived().visitor.module_.get_associated_identifier(stmt);
        }

        decltype(auto) identifier(ebm::WeakStatementRef stmt) const {
            return derived().visitor.module_.get_associated_identifier(stmt);
        }

        decltype(auto) identifier(ebm::ExpressionRef expr) const {
            return derived().visitor.module_.get_associated_identifier(expr);
        }

        decltype(auto) identifier(ebm::TypeRef type) const {
            return derived().visitor.module_.get_associated_identifier(type);
        }

        auto& module() const {
            return derived().visitor.module_;
        }

        // alias for visitor
        auto& config() const {
            return derived().visitor;
        }

        auto get_writer() {
            return derived().visitor.wm.get_writer();
        }

        auto add_writer() {
            return derived().visitor.wm.add_writer();
        }

        decltype(auto) get_entry_point() const {
            return derived().visitor.module_.get_entry_point();
        }

        auto& flags() const {
            return derived().visitor.flags;
        }

        auto& output() {
            return derived().visitor.output;
        }

        decltype(auto) visit(ebm::TypeRef type_ref) const {
            return visit_Type(derived(), type_ref);
        }

        decltype(auto) visit(const ebm::Type& type) const {
            return visit_Type(derived(), type);
        }

        decltype(auto) visit(ebm::ExpressionRef expr_ref) const {
            return visit_Expression(derived(), expr_ref);
        }

        decltype(auto) visit(const ebm::Expression& expr) const {
            return visit_Expression(derived(), expr);
        }

        decltype(auto) visit(ebm::StatementRef stmt_ref) const {
            return visit_Statement(derived(), stmt_ref);
        }

        decltype(auto) visit(const ebm::Statement& stmt) const {
            return visit_Statement(derived(), stmt);
        }

        decltype(auto) visit(const ebm::Block& block) const {
            return visit_Block(derived(), block);
        }

        decltype(auto) visit(const ebm::Expressions& exprs) const {
            return visit_Expressions(derived(), exprs);
        }

        decltype(auto) visit(const ebm::Types& types) const {
            return visit_Types(derived(), types);
        }

        decltype(auto) visit() const
            requires has_item_id<D>
        {
            return visit(derived().item_id);
        }

        decltype(auto) get(ebm::TypeRef type_ref) const {
            return derived().module().get_type(type_ref);
        }

        decltype(auto) get(ebm::StatementRef stmt_ref) const {
            return derived().module().get_statement(stmt_ref);
        }

        decltype(auto) get(ebm::WeakStatementRef stmt_ref) const {
            return derived().module().get_statement(stmt_ref);
        }

        decltype(auto) get(ebm::ExpressionRef expr_ref) const {
            return derived().module().get_expression(expr_ref);
        }

        decltype(auto) get(ebm::StringRef string_ref) const {
            return derived().module().get_string_literal(string_ref);
        }

        decltype(auto) get(ebm::IdentifierRef ident_ref) const {
            return derived().module().get_identifier(ident_ref);
        }

        decltype(auto) get_kind(ebm::TypeRef type_ref) const {
            return derived().module().get_type_kind(type_ref);
        }

        decltype(auto) get_kind(ebm::StatementRef stmt_ref) const {
            return derived().module().get_statement_kind(stmt_ref);
        }

        decltype(auto) get_kind(ebm::WeakStatementRef stmt_ref) const {
            return derived().module().get_statement_kind(from_weak(stmt_ref));
        }

        decltype(auto) get_kind(ebm::ExpressionRef expr_ref) const {
            return derived().module().get_expression_kind(expr_ref);
        }

        bool is(ebm::TypeKind kind, ebm::TypeRef ref) const {
            return get_kind(ref) == kind;
        }

        bool is(ebm::StatementKind kind, ebm::StatementRef ref) const {
            return get_kind(ref) == kind;
        }

        bool is(ebm::StatementKind kind, ebm::WeakStatementRef ref) const {
            return get_kind(from_weak(ref)) == kind;
        }

        bool is(ebm::ExpressionKind kind, ebm::ExpressionRef ref) const {
            return get_kind(ref) == kind;
        }

        bool is(ebm::TypeKind kind) const
            requires has_type<D> || has_item_id_typed<D, ebm::TypeRef>
        {
            if constexpr (has_type<D>) {
                return get_kind(derived().type) == kind;
            }
            else {
                return get_kind(derived().item_id) == kind;
            }
        }

        bool is(ebm::StatementKind kind) const
            requires has_item_id_typed<D, ebm::StatementRef>
        {
            return get_kind(derived().item_id) == kind;
        }

        bool is(ebm::ExpressionKind kind) const
            requires has_item_id_typed<D, ebm::ExpressionRef>
        {
            return get_kind(derived().item_id) == kind;
        }

        template <ebmgen::FieldNames<> V>
        decltype(auto) get_field() const
            requires has_item_id<D>
        {
            return ebmgen::access_field<V>(module(), derived().item_id);
        }

        template <ebmgen::FieldNames<> V>
        decltype(auto) get_field(auto&& root) const {
            return ebmgen::access_field<V>(module(), root);
        }

        template <size_t N, ebmgen::FieldNames<N> V>
        decltype(auto) get_field(auto&& root) const {
            return ebmgen::access_field<N, V>(module(), root);
        }

        template <class R = void, class F>
        R get_impl(F&& f) const {
            return get_visitor_impl<R>(derived(), f);
        }

        static constexpr bool is_before_or_after() {
            constexpr std::string_view name = D::context_name;
            return name.ends_with("_before") || name.ends_with("_after");
        }

        template <class R = void>
        ebmgen::expected<R> visit(auto&& visitor, auto&& c) {
            return visit_Object_adl<R>(visitor, std::forward<decltype(c)>(c), visitor.visitor);
        }
    };

    template <typename T>
    constexpr bool dependent_false = false;
    template <typename Context>
    concept HasVisitorInContext = requires(const Context& ctx) { ctx.visitor; };
    template <typename Context>
    concept HasLegacyVisitorInContext = requires(const Context& ctx) { *ctx.__legacy_compat_ptr; };
    template <typename Result, typename VisitorImpl, typename Context>
    concept HasVisitor = !std::is_base_of_v<ContextBase<std::decay_t<VisitorImpl>>, std::decay_t<VisitorImpl>> && requires(VisitorImpl v, Context c) {
        { v.visit(c) } -> std::convertible_to<ebmgen::expected<Result>>;
    };

    template <typename Context>
    decltype(auto) get_visitor_arg_from_context(Context&& ctx) {
        if constexpr (HasVisitorInContext<Context>) {
            return ctx.visitor;
        }
        else if constexpr (HasLegacyVisitorInContext<Context>) {
            return *ctx.__legacy_compat_ptr;
        }
        else {
            static_assert(dependent_false<Context>, "No visitor found in context");
        }
    }
    template <typename Result, typename UserContext, typename TypeContext>
    auto& get_visitor_from_context(UserContext&& uctx, TypeContext&& ctx) {
        if constexpr (HasVisitor<Result, UserContext, TypeContext>) {
            return uctx;
        }
        else if constexpr (HasVisitorInContext<UserContext>) {
            return *uctx.visitor.__legacy_compat_ptr;
        }
        else if constexpr (HasLegacyVisitorInContext<UserContext>) {
            return *uctx.__legacy_compat_ptr;
        }
        else {
            static_assert(dependent_false<UserContext>, "No visitor found in context");
        }
    }

    template <class Context>
    auto& get_visitor(Context&& ctx) {
        if constexpr (HasVisitorInContext<Context> || HasLegacyVisitorInContext<Context>) {
            return get_visitor_arg_from_context(ctx);
        }
        else {
            return ctx;
        }
    }
}  // namespace ebmcodegen::util