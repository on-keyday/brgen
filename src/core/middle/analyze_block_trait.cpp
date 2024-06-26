/*license*/
#include "analyze_block_trait.h"
#include <core/ast/ast.h>
#include <core/ast/traverse.h>
#include <core/ast/tool/ident.h>

namespace brgen::middle {

    void analyze_expr(const std::shared_ptr<ast::Expr>& expr, auto&& add_trait) {
        if (auto bin = ast::as<ast::Binary>(expr)) {
            if (ast::is_define_op(bin->op)) {
                add_trait(ast::BlockTrait::local_variable);
                analyze_expr(bin->right, add_trait);
            }
            else if (ast::is_assign_op(bin->op) && ast::tool::is_state_variable_ref(bin->left)) {
                add_trait(ast::BlockTrait::write_state);
                analyze_expr(bin->right, add_trait);
            }
            else {
                analyze_expr(bin->left, add_trait);
                analyze_expr(bin->right, add_trait);
            }
        }
        else if (auto io_op = ast::as<ast::IOOperation>(expr)) {
            if (io_op->method == ast::IOMethod::input_backward) {
                add_trait(ast::BlockTrait::backward_input);
            }
            else if (io_op->method == ast::IOMethod::input_peek) {
                add_trait(ast::BlockTrait::static_peek);
            }
            else if (io_op->method == ast::IOMethod::input_get ||
                     io_op->method == ast::IOMethod::output_put) {
                add_trait(ast::BlockTrait::procedural);
            }
        }
        else if (ast::tool::is_state_variable_ref(expr)) {
            add_trait(ast::BlockTrait::read_state);
        }
        else {
            ast::traverse(expr, [&](const std::shared_ptr<ast::Node>& node) {
                if (!ast::as<ast::Expr>(node)) {
                    return;
                }
                analyze_expr(ast::cast_to<ast::Expr>(node), add_trait);
            });
        }
    }

    void analyze_field_argument(const std::shared_ptr<ast::Type>& typ, const std::shared_ptr<ast::FieldArgument>& args, auto&& add_trait, auto&& derive_trait) {
        if (!args) return;
        if (args->peek_value && *args->peek_value) {
            add_trait(ast::BlockTrait::static_peek);
        }
        if (args->arguments.size() > 0) {
            add_trait(ast::BlockTrait::magic_value);
        }
        if (args->assigns.size()) {
            for (auto& assign : args->assigns) {
                analyze_element(assign, add_trait, derive_trait);
            }
        }
    }

    void derive_from_struct_type(ast::StructType* typ, auto&& derive_trait) {
        auto base = typ->base.lock();
        if (auto fmt = ast::as<ast::Format>(base)) {
            derive_trait(fmt->body->block_traits);
        }
    }

    void analyze_field_type(const std::shared_ptr<ast::Type>& type, auto&& add_trait, auto&& derive_trait, bool indirect) {
        if (auto ident = ast::as<ast::IdentType>(type)) {
            analyze_field_type(ident->base.lock(), add_trait, derive_trait, indirect);
        }
        else if (auto enum_ = ast::as<ast::EnumType>(type)) {
            auto base_ty = enum_->base.lock()->base_type;
            if (!base_ty) {
                // pattern: enum without base type
                add_trait(ast::BlockTrait::description_only);
                return;
            }
            analyze_field_type(base_ty, add_trait, derive_trait, indirect);
        }
        else if (auto struct_ = ast::as<ast::StructType>(type)) {
            add_trait(ast::BlockTrait::struct_);
        }
        else if (auto arr_ = ast::as<ast::ArrayType>(type)) {
            if (arr_->length_value) {
                // pattern: fixed-size array
                add_trait(ast::BlockTrait::fixed_array);
            }
            else {
                // pattern: variable-size array
                add_trait(ast::BlockTrait::variable_array);
            }
            analyze_field_type(arr_->element_type, add_trait, derive_trait, true);
        }
        else if (auto int_ = ast::as<ast::IntType>(type)) {
            add_trait(ast::BlockTrait::fixed_primitive);
            if (*int_->bit_size % 8 != 0) {
                // pattern: size of field is not byte-aligned
                add_trait(ast::BlockTrait::bit_field);
            }
            else if (!int_->is_common_supported) {
                add_trait(ast::BlockTrait::uncommon_size);
            }
        }
        else if (auto float_ = ast::as<ast::FloatType>(type)) {
            add_trait(ast::BlockTrait::fixed_primitive);
            if (*float_->bit_size % 8 != 0) {
                // pattern: size of field is not byte-aligned
                add_trait(ast::BlockTrait::bit_field);
            }
            else if (!float_->is_common_supported) {
                add_trait(ast::BlockTrait::uncommon_size);
            }
        }
        else if (auto str = ast::as<ast::StrLiteralType>(type)) {
            add_trait(ast::BlockTrait::magic_value);
        }
    }
    void analyze_block(const std::shared_ptr<ast::IndentBlock>& block);

    void analyze_element(const std::shared_ptr<ast::Node>& elm, auto&& add_trait, auto&& derive_trait) {
        if (auto fmt = ast::as<ast::Format>(elm)) {
            analyze_block(fmt->body);
            // not derived
        }
        else if (auto state = ast::as<ast::State>(elm)) {
            analyze_block(state->body);
            // not derived
        }
        else if (auto field = ast::as<ast::Field>(elm)) {
            auto type = field->field_type;
            analyze_field_type(type, add_trait, derive_trait, false);
            analyze_field_argument(type, field->arguments, add_trait, derive_trait);
        }
        else if (auto fn = ast::as<ast::Function>(elm)) {
            analyze_block(fn->body);
            // not derived
        }
        else if (auto for_ = ast::as<ast::Loop>(elm)) {
            add_trait(ast::BlockTrait::for_loop);
            if (for_->init) {
                analyze_element(for_->init, add_trait, derive_trait);
            }
            if (for_->cond) {
                analyze_element(for_->cond, add_trait, derive_trait);
            }
            if (for_->step) {
                analyze_element(for_->step, add_trait, derive_trait);
            }
            analyze_block(for_->body);
            derive_trait(for_->body->block_traits);
        }
        else if (ast::as<ast::Break>(elm) || ast::as<ast::Continue>(elm) || ast::as<ast::Return>(elm)) {
            add_trait(ast::BlockTrait::control_flow_change);
        }
        else if (auto if_ = ast::as<ast::If>(elm)) {
            add_trait(ast::BlockTrait::conditional);
            if (if_->cond) {
                analyze_expr(if_->cond->expr, add_trait);
            }
            analyze_block(if_->then);
            derive_trait(if_->then->block_traits);
            if (if_->els) {
                if (auto els_ = ast::as<ast::If>(if_->els)) {
                    analyze_element(if_->els, add_trait, derive_trait);
                }
                else if (auto body = ast::as<ast::IndentBlock>(if_->els)) {
                    analyze_block(ast::cast_to<ast::IndentBlock>(if_->els));
                    derive_trait(body->block_traits);
                }
            }
        }
        else if (auto match_ = ast::as<ast::Match>(elm)) {
            add_trait(ast::BlockTrait::conditional);
            if (match_->cond) {
                analyze_expr(match_->cond, add_trait);
            }
            for (auto& case_ : match_->branch) {
                if (case_->cond) {
                    analyze_element(case_->cond->expr, add_trait, derive_trait);
                }
                if (auto block = ast::as<ast::IndentBlock>(case_->then)) {
                    analyze_block(ast::cast_to<ast::IndentBlock>(case_->then));
                    derive_trait(block->block_traits);
                }
                else if (auto scoped = ast::as<ast::ScopedStatement>(case_->then)) {
                    analyze_element(scoped->statement, add_trait, derive_trait);
                }
            }
        }
        else if (auto assert_ = ast::as<ast::Assert>(elm)) {
            add_trait(ast::BlockTrait::assertion);
        }
        else if (auto explicit_error = ast::as<ast::ExplicitError>(elm)) {
            add_trait(ast::BlockTrait::explicit_error);
        }
        else if (auto specify_order = ast::as<ast::SpecifyOrder>(elm)) {
            // pattern: order is dynamically specified
            add_trait(ast::BlockTrait::dynamic_order);
        }
        else if (auto expr = ast::as<ast::Expr>(elm)) {
            analyze_expr(ast::cast_to<ast::Expr>(elm), add_trait);
        }
    }

    void analyze_block(const std::shared_ptr<ast::IndentBlock>& block) {
        auto add_trait = [&](ast::BlockTrait t) {
            block->block_traits = ast::BlockTrait(size_t(block->block_traits) | size_t(t));
        };
        auto derive_trait = [&](ast::BlockTrait t) {
            // some type of trait are not derived
            auto to_derive = size_t(t) & ~size_t(ast::BlockTrait::bit_stream);
            add_trait(ast::BlockTrait(to_derive));
        };
        if (block->struct_type->bit_alignment != ast::BitAlignment::byte_aligned) {
            // pattern: struct is not byte-aligned
            add_trait(ast::BlockTrait::bit_stream);
        }

        for (auto& elm : block->elements) {
            analyze_element(elm, add_trait, derive_trait);
        }
    }

    void analyze_block_trait_impl(const std::shared_ptr<ast::Program>& trait) {
        for (auto& elm : trait->elements) {
            analyze_element(elm, [&](ast::BlockTrait t) {}, [&](ast::BlockTrait t) {});
        }
    }

    void analyze_block_trait(const std::shared_ptr<ast::Program>& trait) {
        analyze_block_trait_impl(trait);
        analyze_block_trait_impl(trait);
    }
}  // namespace brgen::middle
